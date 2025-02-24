// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This has to be included first.
// See http://code.google.com/p/googletest/issues/detail?id=371
#include "testing/gtest/include/gtest/gtest.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <memory>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/path_service.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "media/base/test_data_util.h"
#include "media/filters/jpeg_parser.h"
#include "media/gpu/video_accelerator_unittest_helpers.h"
#include "media/video/jpeg_decode_accelerator.h"
#include "third_party/libyuv/include/libyuv.h"
#include "ui/gfx/codec/jpeg_codec.h"

#if defined(OS_CHROMEOS)
#if defined(USE_V4L2_CODEC)
#include "media/gpu/v4l2_device.h"
#include "media/gpu/v4l2_jpeg_decode_accelerator.h"
#endif
#if defined(ARCH_CPU_X86_FAMILY)
#include "media/gpu/vaapi_jpeg_decode_accelerator.h"
#include "media/gpu/vaapi_wrapper.h"
#endif
#endif

namespace media {
namespace {

// Default test image file.
const base::FilePath::CharType* kDefaultJpegFilename =
    FILE_PATH_LITERAL("peach_pi-1280x720.jpg");
// Decide to save decode results to files or not. Output files will be saved
// in the same directory with unittest. File name is like input file but
// changing the extension to "yuv".
bool g_save_to_file = false;
// Threshold for mean absolute difference of hardware and software decode.
// Absolute difference is to calculate the difference between each pixel in two
// images. This is used for measuring of the similarity of two images.
const double kDecodeSimilarityThreshold = 1.0;

// Environment to create test data for all test cases.
class JpegDecodeAcceleratorTestEnvironment;
JpegDecodeAcceleratorTestEnvironment* g_env;

struct TestImageFile {
  explicit TestImageFile(const base::FilePath::StringType& filename)
      : filename(filename) {}

  base::FilePath::StringType filename;

  // The input content of |filename|.
  std::string data_str;

  JpegParseResult parse_result;
  gfx::Size visible_size;
  size_t output_size;
};

enum ClientState {
  CS_CREATED,
  CS_INITIALIZED,
  CS_DECODE_PASS,
  CS_ERROR,
};

class JpegClient : public JpegDecodeAccelerator::Client {
 public:
  JpegClient(const std::vector<TestImageFile*>& test_image_files,
             ClientStateNotification<ClientState>* note);
  ~JpegClient() override;
  void CreateJpegDecoder();
  void DestroyJpegDecoder();
  void StartDecode(int32_t bitstream_buffer_id);

  // JpegDecodeAccelerator::Client implementation.
  void VideoFrameReady(int32_t bitstream_buffer_id) override;
  void NotifyError(int32_t bitstream_buffer_id,
                   JpegDecodeAccelerator::Error error) override;

 private:
  void PrepareMemory(int32_t bitstream_buffer_id);
  void SetState(ClientState new_state);
  void SaveToFile(int32_t bitstream_buffer_id);
  bool GetSoftwareDecodeResult(int32_t bitstream_buffer_id);

  // Calculate mean absolute difference of hardware and software decode results
  // to check the similarity.
  double GetMeanAbsoluteDifference(int32_t bitstream_buffer_id);

  // JpegClient doesn't own |test_image_files_|.
  const std::vector<TestImageFile*>& test_image_files_;

  std::unique_ptr<JpegDecodeAccelerator> decoder_;
  ClientState state_;

  // Used to notify another thread about the state. JpegClient does not own
  // this.
  ClientStateNotification<ClientState>* note_;

  // Mapped memory of input file.
  std::unique_ptr<base::SharedMemory> in_shm_;
  // Mapped memory of output buffer from hardware decoder.
  std::unique_ptr<base::SharedMemory> hw_out_shm_;
  // Mapped memory of output buffer from software decoder.
  std::unique_ptr<base::SharedMemory> sw_out_shm_;

  DISALLOW_COPY_AND_ASSIGN(JpegClient);
};

JpegClient::JpegClient(const std::vector<TestImageFile*>& test_image_files,
                       ClientStateNotification<ClientState>* note)
    : test_image_files_(test_image_files), state_(CS_CREATED), note_(note) {}

JpegClient::~JpegClient() {}

void JpegClient::CreateJpegDecoder() {
#if defined(OS_CHROMEOS) && defined(ARCH_CPU_X86_FAMILY)
  decoder_.reset(
      new VaapiJpegDecodeAccelerator(base::ThreadTaskRunnerHandle::Get()));
#elif defined(OS_CHROMEOS) && defined(USE_V4L2_CODEC)
  scoped_refptr<V4L2Device> device = V4L2Device::Create();
  if (!device.get()) {
    LOG(ERROR) << "V4L2Device::Create failed";
    SetState(CS_ERROR);
    return;
  }
  decoder_.reset(new V4L2JpegDecodeAccelerator(
      device, base::ThreadTaskRunnerHandle::Get()));
#else
#error The JpegDecodeAccelerator is not supported on this platform.
#endif
  if (!decoder_->Initialize(this)) {
    LOG(ERROR) << "JpegDecodeAccelerator::Initialize() failed";
    SetState(CS_ERROR);
    return;
  }
  SetState(CS_INITIALIZED);
}

void JpegClient::DestroyJpegDecoder() {
  decoder_.reset();
}

void JpegClient::VideoFrameReady(int32_t bitstream_buffer_id) {
  if (!GetSoftwareDecodeResult(bitstream_buffer_id)) {
    SetState(CS_ERROR);
    return;
  }
  if (g_save_to_file) {
    SaveToFile(bitstream_buffer_id);
  }

  double difference = GetMeanAbsoluteDifference(bitstream_buffer_id);
  if (difference <= kDecodeSimilarityThreshold) {
    SetState(CS_DECODE_PASS);
  } else {
    LOG(ERROR) << "The mean absolute difference between software and hardware "
               << "decode is " << difference;
    SetState(CS_ERROR);
  }
}

void JpegClient::NotifyError(int32_t bitstream_buffer_id,
                             JpegDecodeAccelerator::Error error) {
  LOG(ERROR) << "Notifying of error " << error << " for buffer id "
             << bitstream_buffer_id;
  SetState(CS_ERROR);
}

void JpegClient::PrepareMemory(int32_t bitstream_buffer_id) {
  TestImageFile* image_file = test_image_files_[bitstream_buffer_id];

  size_t input_size = image_file->data_str.size();
  if (!in_shm_.get() || input_size > in_shm_->mapped_size()) {
    in_shm_.reset(new base::SharedMemory);
    LOG_ASSERT(in_shm_->CreateAndMapAnonymous(input_size));
  }
  memcpy(in_shm_->memory(), image_file->data_str.data(), input_size);

  if (!hw_out_shm_.get() ||
      image_file->output_size > hw_out_shm_->mapped_size()) {
    hw_out_shm_.reset(new base::SharedMemory);
    LOG_ASSERT(hw_out_shm_->CreateAndMapAnonymous(image_file->output_size));
  }
  memset(hw_out_shm_->memory(), 0, image_file->output_size);

  if (!sw_out_shm_.get() ||
      image_file->output_size > sw_out_shm_->mapped_size()) {
    sw_out_shm_.reset(new base::SharedMemory);
    LOG_ASSERT(sw_out_shm_->CreateAndMapAnonymous(image_file->output_size));
  }
  memset(sw_out_shm_->memory(), 0, image_file->output_size);
}

void JpegClient::SetState(ClientState new_state) {
  DVLOG(2) << "Changing state " << state_ << "->" << new_state;
  note_->Notify(new_state);
  state_ = new_state;
}

void JpegClient::SaveToFile(int32_t bitstream_buffer_id) {
  TestImageFile* image_file = test_image_files_[bitstream_buffer_id];

  base::FilePath in_filename(image_file->filename);
  base::FilePath out_filename = in_filename.ReplaceExtension(".yuv");
  int size = base::checked_cast<int>(image_file->output_size);
  ASSERT_EQ(size,
            base::WriteFile(out_filename,
                            static_cast<char*>(hw_out_shm_->memory()), size));
}

double JpegClient::GetMeanAbsoluteDifference(int32_t bitstream_buffer_id) {
  TestImageFile* image_file = test_image_files_[bitstream_buffer_id];

  double total_difference = 0;
  uint8_t* hw_ptr = static_cast<uint8_t*>(hw_out_shm_->memory());
  uint8_t* sw_ptr = static_cast<uint8_t*>(sw_out_shm_->memory());
  for (size_t i = 0; i < image_file->output_size; i++)
    total_difference += std::abs(hw_ptr[i] - sw_ptr[i]);
  return total_difference / image_file->output_size;
}

void JpegClient::StartDecode(int32_t bitstream_buffer_id) {
  DCHECK_LT(static_cast<size_t>(bitstream_buffer_id), test_image_files_.size());
  TestImageFile* image_file = test_image_files_[bitstream_buffer_id];

  PrepareMemory(bitstream_buffer_id);

  base::SharedMemoryHandle dup_handle;
  dup_handle = base::SharedMemory::DuplicateHandle(in_shm_->handle());
  BitstreamBuffer bitstream_buffer(bitstream_buffer_id, dup_handle,
                                   image_file->data_str.size());
  scoped_refptr<VideoFrame> out_frame_ = VideoFrame::WrapExternalSharedMemory(
      PIXEL_FORMAT_I420, image_file->visible_size,
      gfx::Rect(image_file->visible_size), image_file->visible_size,
      static_cast<uint8_t*>(hw_out_shm_->memory()), image_file->output_size,
      hw_out_shm_->handle(), 0, base::TimeDelta());
  LOG_ASSERT(out_frame_.get());
  decoder_->Decode(bitstream_buffer, out_frame_);
}

bool JpegClient::GetSoftwareDecodeResult(int32_t bitstream_buffer_id) {
  VideoPixelFormat format = PIXEL_FORMAT_I420;
  TestImageFile* image_file = test_image_files_[bitstream_buffer_id];

  uint8_t* yplane = static_cast<uint8_t*>(sw_out_shm_->memory());
  uint8_t* uplane = yplane +
                    VideoFrame::PlaneSize(format, VideoFrame::kYPlane,
                                          image_file->visible_size)
                        .GetArea();
  uint8_t* vplane = uplane +
                    VideoFrame::PlaneSize(format, VideoFrame::kUPlane,
                                          image_file->visible_size)
                        .GetArea();
  int yplane_stride = image_file->visible_size.width();
  int uv_plane_stride = yplane_stride / 2;

  if (libyuv::ConvertToI420(
          static_cast<uint8_t*>(in_shm_->memory()),
          image_file->data_str.size(),
          yplane,
          yplane_stride,
          uplane,
          uv_plane_stride,
          vplane,
          uv_plane_stride,
          0,
          0,
          image_file->visible_size.width(),
          image_file->visible_size.height(),
          image_file->visible_size.width(),
          image_file->visible_size.height(),
          libyuv::kRotate0,
          libyuv::FOURCC_MJPG) != 0) {
    LOG(ERROR) << "Software decode " << image_file->filename << " failed.";
    return false;
  }
  return true;
}

class JpegDecodeAcceleratorTestEnvironment : public ::testing::Environment {
 public:
  JpegDecodeAcceleratorTestEnvironment(
      const base::FilePath::CharType* jpeg_filenames) {
    user_jpeg_filenames_ =
        jpeg_filenames ? jpeg_filenames : kDefaultJpegFilename;
  }
  void SetUp() override;
  void TearDown() override;

  // Create all black test image with |width| and |height| size.
  bool CreateTestJpegImage(int width, int height, base::FilePath* filename);

  // Read image from |filename| to |image_data|.
  void ReadTestJpegImage(base::FilePath& filename, TestImageFile* image_data);

  // Parsed data of |test_1280x720_jpeg_file_|.
  std::unique_ptr<TestImageFile> image_data_1280x720_black_;
  // Parsed data of |test_640x368_jpeg_file_|.
  std::unique_ptr<TestImageFile> image_data_640x368_black_;
  // Parsed data of |test_640x360_jpeg_file_|.
  std::unique_ptr<TestImageFile> image_data_640x360_black_;
  // Parsed data of "peach_pi-1280x720.jpg".
  std::unique_ptr<TestImageFile> image_data_1280x720_default_;
  // Parsed data of failure image.
  std::unique_ptr<TestImageFile> image_data_invalid_;
  // Parsed data from command line.
  ScopedVector<TestImageFile> image_data_user_;

 private:
  const base::FilePath::CharType* user_jpeg_filenames_;

  // Used for InputSizeChange test case. The image size should be smaller than
  // |kDefaultJpegFilename|.
  base::FilePath test_1280x720_jpeg_file_;
  // Used for ResolutionChange test case.
  base::FilePath test_640x368_jpeg_file_;
  // Used for testing some drivers which will align the output resolution to a
  // multiple of 16. 640x360 will be aligned to 640x368.
  base::FilePath test_640x360_jpeg_file_;
};

void JpegDecodeAcceleratorTestEnvironment::SetUp() {
  ASSERT_TRUE(CreateTestJpegImage(1280, 720, &test_1280x720_jpeg_file_));
  ASSERT_TRUE(CreateTestJpegImage(640, 368, &test_640x368_jpeg_file_));
  ASSERT_TRUE(CreateTestJpegImage(640, 360, &test_640x360_jpeg_file_));

  image_data_1280x720_black_.reset(
      new TestImageFile(test_1280x720_jpeg_file_.value()));
  ASSERT_NO_FATAL_FAILURE(ReadTestJpegImage(test_1280x720_jpeg_file_,
                                            image_data_1280x720_black_.get()));

  image_data_640x368_black_.reset(
      new TestImageFile(test_640x368_jpeg_file_.value()));
  ASSERT_NO_FATAL_FAILURE(ReadTestJpegImage(test_640x368_jpeg_file_,
                                            image_data_640x368_black_.get()));

  image_data_640x360_black_.reset(
      new TestImageFile(test_640x360_jpeg_file_.value()));
  ASSERT_NO_FATAL_FAILURE(ReadTestJpegImage(test_640x360_jpeg_file_,
                                            image_data_640x360_black_.get()));

  base::FilePath default_jpeg_file = GetTestDataFilePath(kDefaultJpegFilename);
  image_data_1280x720_default_.reset(new TestImageFile(kDefaultJpegFilename));
  ASSERT_NO_FATAL_FAILURE(
      ReadTestJpegImage(default_jpeg_file, image_data_1280x720_default_.get()));

  image_data_invalid_.reset(new TestImageFile("failure.jpg"));
  image_data_invalid_->data_str.resize(100, 0);
  image_data_invalid_->visible_size.SetSize(1280, 720);
  image_data_invalid_->output_size = VideoFrame::AllocationSize(
      PIXEL_FORMAT_I420, image_data_invalid_->visible_size);

  // |user_jpeg_filenames_| may include many files and use ';' as delimiter.
  std::vector<base::FilePath::StringType> filenames = base::SplitString(
      user_jpeg_filenames_, base::FilePath::StringType(1, ';'),
      base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  for (const auto& filename : filenames) {
    base::FilePath input_file = GetTestDataFilePath(filename);
    TestImageFile* image_data = new TestImageFile(filename);
    ASSERT_NO_FATAL_FAILURE(ReadTestJpegImage(input_file, image_data));
    image_data_user_.push_back(image_data);
  }
}

void JpegDecodeAcceleratorTestEnvironment::TearDown() {
  base::DeleteFile(test_1280x720_jpeg_file_, false);
  base::DeleteFile(test_640x368_jpeg_file_, false);
  base::DeleteFile(test_640x360_jpeg_file_, false);
}

bool JpegDecodeAcceleratorTestEnvironment::CreateTestJpegImage(
    int width,
    int height,
    base::FilePath* filename) {
  const int kBytesPerPixel = 3;
  const int kJpegQuality = 100;
  std::vector<unsigned char> input_buffer(width * height * kBytesPerPixel);
  std::vector<unsigned char> encoded;
  if (!gfx::JPEGCodec::Encode(&input_buffer[0], gfx::JPEGCodec::FORMAT_RGB,
                              width, height, width * kBytesPerPixel,
                              kJpegQuality, &encoded)) {
    return false;
  }

  LOG_ASSERT(base::CreateTemporaryFile(filename));
  EXPECT_TRUE(base::AppendToFile(
      *filename, reinterpret_cast<char*>(&encoded[0]), encoded.size()));
  return true;
}

void JpegDecodeAcceleratorTestEnvironment::ReadTestJpegImage(
    base::FilePath& input_file,
    TestImageFile* image_data) {
  ASSERT_TRUE(base::ReadFileToString(input_file, &image_data->data_str));

  ASSERT_TRUE(ParseJpegPicture(
      reinterpret_cast<const uint8_t*>(image_data->data_str.data()),
      image_data->data_str.size(), &image_data->parse_result));
  image_data->visible_size.SetSize(
      image_data->parse_result.frame_header.visible_width,
      image_data->parse_result.frame_header.visible_height);
  image_data->output_size =
      VideoFrame::AllocationSize(PIXEL_FORMAT_I420, image_data->visible_size);
}

class JpegDecodeAcceleratorTest : public ::testing::Test {
 protected:
  JpegDecodeAcceleratorTest() {}

  void TestDecode(size_t num_concurrent_decoders);

  // The elements of |test_image_files_| are owned by
  // JpegDecodeAcceleratorTestEnvironment.
  std::vector<TestImageFile*> test_image_files_;
  std::vector<ClientState> expected_status_;

 protected:
  DISALLOW_COPY_AND_ASSIGN(JpegDecodeAcceleratorTest);
};

void JpegDecodeAcceleratorTest::TestDecode(size_t num_concurrent_decoders) {
  LOG_ASSERT(test_image_files_.size() == expected_status_.size());
  base::Thread decoder_thread("DecoderThread");
  ASSERT_TRUE(decoder_thread.Start());

  ScopedVector<ClientStateNotification<ClientState>> notes;
  ScopedVector<JpegClient> clients;

  for (size_t i = 0; i < num_concurrent_decoders; i++) {
    notes.push_back(new ClientStateNotification<ClientState>());
    clients.push_back(new JpegClient(test_image_files_, notes.back()));
    decoder_thread.task_runner()->PostTask(
        FROM_HERE, base::Bind(&JpegClient::CreateJpegDecoder,
                              base::Unretained(clients.back())));
    ASSERT_EQ(notes[i]->Wait(), CS_INITIALIZED);
  }

  for (size_t index = 0; index < test_image_files_.size(); index++) {
    for (size_t i = 0; i < num_concurrent_decoders; i++) {
      decoder_thread.task_runner()->PostTask(
          FROM_HERE, base::Bind(&JpegClient::StartDecode,
                                base::Unretained(clients[i]), index));
    }
    for (size_t i = 0; i < num_concurrent_decoders; i++) {
      ASSERT_EQ(notes[i]->Wait(), expected_status_[index]);
    }
  }

  for (size_t i = 0; i < num_concurrent_decoders; i++) {
    decoder_thread.task_runner()->PostTask(
        FROM_HERE, base::Bind(&JpegClient::DestroyJpegDecoder,
                              base::Unretained(clients[i])));
  }
  decoder_thread.Stop();
}

TEST_F(JpegDecodeAcceleratorTest, SimpleDecode) {
  for (auto* image : g_env->image_data_user_) {
    test_image_files_.push_back(image);
    expected_status_.push_back(CS_DECODE_PASS);
  }
  TestDecode(1);
}

TEST_F(JpegDecodeAcceleratorTest, MultipleDecoders) {
  for (auto* image : g_env->image_data_user_) {
    test_image_files_.push_back(image);
    expected_status_.push_back(CS_DECODE_PASS);
  }
  TestDecode(3);
}

TEST_F(JpegDecodeAcceleratorTest, InputSizeChange) {
  // The size of |image_data_1280x720_black_| is smaller than
  // |image_data_1280x720_default_|.
  test_image_files_.push_back(g_env->image_data_1280x720_black_.get());
  test_image_files_.push_back(g_env->image_data_1280x720_default_.get());
  test_image_files_.push_back(g_env->image_data_1280x720_black_.get());
  for (size_t i = 0; i < test_image_files_.size(); i++)
    expected_status_.push_back(CS_DECODE_PASS);
  TestDecode(1);
}

TEST_F(JpegDecodeAcceleratorTest, ResolutionChange) {
  test_image_files_.push_back(g_env->image_data_640x368_black_.get());
  test_image_files_.push_back(g_env->image_data_1280x720_default_.get());
  test_image_files_.push_back(g_env->image_data_640x368_black_.get());
  for (size_t i = 0; i < test_image_files_.size(); i++)
    expected_status_.push_back(CS_DECODE_PASS);
  TestDecode(1);
}

TEST_F(JpegDecodeAcceleratorTest, CodedSizeAlignment) {
  test_image_files_.push_back(g_env->image_data_640x360_black_.get());
  expected_status_.push_back(CS_DECODE_PASS);
  TestDecode(1);
}

TEST_F(JpegDecodeAcceleratorTest, FailureJpeg) {
  test_image_files_.push_back(g_env->image_data_invalid_.get());
  expected_status_.push_back(CS_ERROR);
  TestDecode(1);
}

TEST_F(JpegDecodeAcceleratorTest, KeepDecodeAfterFailure) {
  test_image_files_.push_back(g_env->image_data_invalid_.get());
  test_image_files_.push_back(g_env->image_data_1280x720_default_.get());
  expected_status_.push_back(CS_ERROR);
  expected_status_.push_back(CS_DECODE_PASS);
  TestDecode(1);
}

}  // namespace
}  // namespace media

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  base::CommandLine::Init(argc, argv);
  base::ShadowingAtExitManager at_exit_manager;

  // Needed to enable DVLOG through --vmodule.
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  LOG_ASSERT(logging::InitLogging(settings));

  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  DCHECK(cmd_line);

  const base::FilePath::CharType* jpeg_filenames = nullptr;
  base::CommandLine::SwitchMap switches = cmd_line->GetSwitches();
  for (base::CommandLine::SwitchMap::const_iterator it = switches.begin();
       it != switches.end(); ++it) {
    // jpeg_filenames can include one or many files and use ';' as delimiter.
    if (it->first == "jpeg_filenames") {
      jpeg_filenames = it->second.c_str();
      continue;
    }
    if (it->first == "save_to_file") {
      media::g_save_to_file = true;
      continue;
    }
    if (it->first == "v" || it->first == "vmodule")
      continue;
    if (it->first == "h" || it->first == "help")
      continue;
    LOG(ERROR) << "Unexpected switch: " << it->first << ":" << it->second;
    return -EINVAL;
  }
#if defined(OS_CHROMEOS) && defined(ARCH_CPU_X86_FAMILY)
  media::VaapiWrapper::PreSandboxInitialization();
#endif

  media::g_env = reinterpret_cast<media::JpegDecodeAcceleratorTestEnvironment*>(
      testing::AddGlobalTestEnvironment(
          new media::JpegDecodeAcceleratorTestEnvironment(jpeg_filenames)));

  return RUN_ALL_TESTS();
}
