// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.support.test.filters.SmallTest;

import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.JavaBridgeTestCommon.Controller;

/**
 * Part of the test suite for the Java Bridge. This class tests that we correctly convert
 * JavaScript arrays to Java arrays when passing them to the methods of injected Java objects.
 *
 * The conversions should follow
 * http://jdk6.java.net/plugin2/liveconnect/#JS_JAVA_CONVERSIONS. Places in
 * which the implementation differs from the spec are marked with
 * LIVECONNECT_COMPLIANCE.
 * FIXME: Consider making our implementation more compliant, if it will not
 * break backwards-compatibility. See b/4408210.
 */
public class JavaBridgeArrayCoercionTest extends JavaBridgeTestBase {
    @SuppressFBWarnings("CHROMIUM_SYNCHRONIZED_METHOD")
    private static class TestObject extends Controller {
        private final Object mObjectInstance;
        private final CustomType mCustomTypeInstance;

        private boolean[] mBooleanArray;
        private byte[] mByteArray;
        private char[] mCharArray;
        private short[] mShortArray;
        private int[] mIntArray;
        private long[] mLongArray;
        private float[] mFloatArray;
        private double[] mDoubleArray;
        private String[] mStringArray;
        private Object[] mObjectArray;
        private CustomType[] mCustomTypeArray;

        public TestObject() {
            mObjectInstance = new Object();
            mCustomTypeInstance = new CustomType();
        }

        public Object getObjectInstance() {
            return mObjectInstance;
        }
        public CustomType getCustomTypeInstance() {
            return mCustomTypeInstance;
        }

        public synchronized void setBooleanArray(boolean[] x) {
            mBooleanArray = x;
            notifyResultIsReady();
        }
        public synchronized void setByteArray(byte[] x) {
            mByteArray = x;
            notifyResultIsReady();
        }
        public synchronized void setCharArray(char[] x) {
            mCharArray = x;
            notifyResultIsReady();
        }
        public synchronized void setShortArray(short[] x) {
            mShortArray = x;
            notifyResultIsReady();
        }
        public synchronized void setIntArray(int[] x) {
            mIntArray = x;
            notifyResultIsReady();
        }
        public synchronized void setLongArray(long[] x) {
            mLongArray = x;
            notifyResultIsReady();
        }
        public synchronized void setFloatArray(float[] x) {
            mFloatArray = x;
            notifyResultIsReady();
        }
        public synchronized void setDoubleArray(double[] x) {
            mDoubleArray = x;
            notifyResultIsReady();
        }
        public synchronized void setStringArray(String[] x) {
            mStringArray = x;
            notifyResultIsReady();
        }
        public synchronized void setObjectArray(Object[] x) {
            mObjectArray = x;
            notifyResultIsReady();
        }
        public synchronized void setCustomTypeArray(CustomType[] x) {
            mCustomTypeArray = x;
            notifyResultIsReady();
        }

        public synchronized boolean[] waitForBooleanArray() {
            waitForResult();
            return mBooleanArray;
        }
        public synchronized byte[] waitForByteArray() {
            waitForResult();
            return mByteArray;
        }
        public synchronized char[] waitForCharArray() {
            waitForResult();
            return mCharArray;
        }
        public synchronized short[] waitForShortArray() {
            waitForResult();
            return mShortArray;
        }
        public synchronized int[] waitForIntArray() {
            waitForResult();
            return mIntArray;
        }
        public synchronized long[] waitForLongArray() {
            waitForResult();
            return mLongArray;
        }
        public synchronized float[] waitForFloatArray() {
            waitForResult();
            return mFloatArray;
        }
        public synchronized double[] waitForDoubleArray() {
            waitForResult();
            return mDoubleArray;
        }
        public synchronized String[] waitForStringArray() {
            waitForResult();
            return mStringArray;
        }
        public synchronized Object[] waitForObjectArray() {
            waitForResult();
            return mObjectArray;
        }
        public synchronized CustomType[] waitForCustomTypeArray() {
            waitForResult();
            return mCustomTypeArray;
        }
    }

    // Two custom types used when testing passing objects.
    private static class CustomType {
    }

    private TestObject mTestObject;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mTestObject = new TestObject();
        injectObjectAndReload(mTestObject, "testObject");
    }

    // Note that all tests use a single element array for simplicity. We test
    // multiple elements elsewhere.

    // Test passing an array of JavaScript numbers in the int32 range to a
    // method which takes a Java array.
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testPassNumberInt32() throws Throwable {
        executeJavaScript("testObject.setBooleanArray([0]);");
        assertFalse(mTestObject.waitForBooleanArray()[0]);
        // LIVECONNECT_COMPLIANCE: Should convert to boolean.
        executeJavaScript("testObject.setBooleanArray([42]);");
        assertFalse(mTestObject.waitForBooleanArray()[0]);

        executeJavaScript("testObject.setByteArray([42]);");
        assertEquals(42, mTestObject.waitForByteArray()[0]);

        executeJavaScript("testObject.setCharArray([42]);");
        assertEquals(42, mTestObject.waitForCharArray()[0]);

        executeJavaScript("testObject.setShortArray([42]);");
        assertEquals(42, mTestObject.waitForShortArray()[0]);

        executeJavaScript("testObject.setIntArray([42]);");
        assertEquals(42, mTestObject.waitForIntArray()[0]);

        executeJavaScript("testObject.setLongArray([42]);");
        assertEquals(42L, mTestObject.waitForLongArray()[0]);

        executeJavaScript("testObject.setFloatArray([42]);");
        assertEquals(42.0f, mTestObject.waitForFloatArray()[0]);

        executeJavaScript("testObject.setDoubleArray([42]);");
        assertEquals(42.0, mTestObject.waitForDoubleArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should create array and create instances of java.lang.Number.
        executeJavaScript("testObject.setObjectArray([42]);");
        assertNull(mTestObject.waitForObjectArray());

        // LIVECONNECT_COMPLIANCE: Should create instances of java.lang.String.
        executeJavaScript("testObject.setStringArray([42]);");
        assertNull(mTestObject.waitForStringArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        executeJavaScript("testObject.setCustomTypeArray([42]);");
        assertNull(mTestObject.waitForCustomTypeArray());
    }

    // Test passing an array of JavaScript numbers in the double range to a
    // method which takes a Java array.
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testPassNumberDouble() throws Throwable {
        // LIVECONNECT_COMPLIANCE: Should convert to boolean.
        executeJavaScript("testObject.setBooleanArray([42.1]);");
        assertFalse(mTestObject.waitForBooleanArray()[0]);

        executeJavaScript("testObject.setByteArray([42.1]);");
        assertEquals(42, mTestObject.waitForByteArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should convert to numeric char value.
        executeJavaScript("testObject.setCharArray([42.1]);");
        assertEquals('\u0000', mTestObject.waitForCharArray()[0]);

        executeJavaScript("testObject.setShortArray([42.1]);");
        assertEquals(42, mTestObject.waitForShortArray()[0]);

        executeJavaScript("testObject.setIntArray([42.1]);");
        assertEquals(42, mTestObject.waitForIntArray()[0]);

        executeJavaScript("testObject.setLongArray([42.1]);");
        assertEquals(42L, mTestObject.waitForLongArray()[0]);

        executeJavaScript("testObject.setFloatArray([42.1]);");
        assertEquals(42.1f, mTestObject.waitForFloatArray()[0]);

        executeJavaScript("testObject.setDoubleArray([42.1]);");
        assertEquals(42.1, mTestObject.waitForDoubleArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should create array and create instances of java.lang.Number.
        executeJavaScript("testObject.setObjectArray([42.1]);");
        assertNull(mTestObject.waitForObjectArray());

        // LIVECONNECT_COMPLIANCE: Should create instances of java.lang.String.
        executeJavaScript("testObject.setStringArray([42.1]);");
        assertNull(mTestObject.waitForStringArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        executeJavaScript("testObject.setCustomTypeArray([42.1]);");
        assertNull(mTestObject.waitForCustomTypeArray());
    }

    // Test passing an array of JavaScript NaN values to a method which takes a
    // Java array.
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testPassNumberNaN() throws Throwable {
        executeJavaScript("testObject.setBooleanArray([Number.NaN]);");
        assertFalse(mTestObject.waitForBooleanArray()[0]);

        executeJavaScript("testObject.setByteArray([Number.NaN]);");
        assertEquals(0, mTestObject.waitForByteArray()[0]);

        executeJavaScript("testObject.setCharArray([Number.NaN]);");
        assertEquals('\u0000', mTestObject.waitForCharArray()[0]);

        executeJavaScript("testObject.setShortArray([Number.NaN]);");
        assertEquals(0, mTestObject.waitForShortArray()[0]);

        executeJavaScript("testObject.setIntArray([Number.NaN]);");
        assertEquals(0, mTestObject.waitForIntArray()[0]);

        executeJavaScript("testObject.setLongArray([Number.NaN]);");
        assertEquals(0L, mTestObject.waitForLongArray()[0]);

        executeJavaScript("testObject.setFloatArray([Number.NaN]);");
        assertEquals(Float.NaN, mTestObject.waitForFloatArray()[0]);

        executeJavaScript("testObject.setDoubleArray([Number.NaN]);");
        assertEquals(Double.NaN, mTestObject.waitForDoubleArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should create array and create instances of java.lang.Number.
        executeJavaScript("testObject.setObjectArray([Number.NaN]);");
        assertNull(mTestObject.waitForObjectArray());

        // LIVECONNECT_COMPLIANCE: Should create instances of java.lang.String.
        executeJavaScript("testObject.setStringArray([Number.NaN]);");
        assertNull(mTestObject.waitForStringArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        executeJavaScript("testObject.setCustomTypeArray([Number.NaN]);");
        assertNull(mTestObject.waitForCustomTypeArray());
    }

    // Test passing an array of JavaScript infinity values to a method which
    // takes a Java array.
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testPassNumberInfinity() throws Throwable {
        executeJavaScript("testObject.setBooleanArray([Infinity]);");
        assertFalse(mTestObject.waitForBooleanArray()[0]);

        executeJavaScript("testObject.setByteArray([Infinity]);");
        assertEquals(-1, mTestObject.waitForByteArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should convert to maximum numeric char value.
        executeJavaScript("testObject.setCharArray([Infinity]);");
        assertEquals('\u0000', mTestObject.waitForCharArray()[0]);

        executeJavaScript("testObject.setShortArray([Infinity]);");
        assertEquals(-1, mTestObject.waitForShortArray()[0]);

        executeJavaScript("testObject.setIntArray([Infinity]);");
        assertEquals(Integer.MAX_VALUE, mTestObject.waitForIntArray()[0]);

        executeJavaScript("testObject.setLongArray([Infinity]);");
        assertEquals(Long.MAX_VALUE, mTestObject.waitForLongArray()[0]);

        executeJavaScript("testObject.setFloatArray([Infinity]);");
        assertEquals(Float.POSITIVE_INFINITY, mTestObject.waitForFloatArray()[0]);

        executeJavaScript("testObject.setDoubleArray([Infinity]);");
        assertEquals(Double.POSITIVE_INFINITY, mTestObject.waitForDoubleArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should create array and create instances of java.lang.Number.
        executeJavaScript("testObject.setObjectArray([Infinity]);");
        assertNull(mTestObject.waitForObjectArray());

        // LIVECONNECT_COMPLIANCE: Should create instances of java.lang.String.
        executeJavaScript("testObject.setStringArray([Infinity]);");
        assertNull(mTestObject.waitForStringArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        executeJavaScript("testObject.setCustomTypeArray([Infinity]);");
        assertNull(mTestObject.waitForCustomTypeArray());
    }

    // Test passing an array of JavaScript boolean values to a method which
    // takes a Java array.
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testPassBoolean() throws Throwable {
        executeJavaScript("testObject.setBooleanArray([true]);");
        assertTrue(mTestObject.waitForBooleanArray()[0]);
        executeJavaScript("testObject.setBooleanArray([false]);");
        assertFalse(mTestObject.waitForBooleanArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should be 1.
        executeJavaScript("testObject.setByteArray([true]);");
        assertEquals(0, mTestObject.waitForByteArray()[0]);
        executeJavaScript("testObject.setByteArray([false]);");
        assertEquals(0, mTestObject.waitForByteArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should convert to numeric char value 1.
        executeJavaScript("testObject.setCharArray([true]);");
        assertEquals('\u0000', mTestObject.waitForCharArray()[0]);
        executeJavaScript("testObject.setCharArray([false]);");
        assertEquals('\u0000', mTestObject.waitForCharArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should be 1.
        executeJavaScript("testObject.setShortArray([true]);");
        assertEquals(0, mTestObject.waitForShortArray()[0]);
        executeJavaScript("testObject.setShortArray([false]);");
        assertEquals(0, mTestObject.waitForShortArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should be 1.
        executeJavaScript("testObject.setIntArray([true]);");
        assertEquals(0, mTestObject.waitForIntArray()[0]);
        executeJavaScript("testObject.setIntArray([false]);");
        assertEquals(0, mTestObject.waitForIntArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should be 1.
        executeJavaScript("testObject.setLongArray([true]);");
        assertEquals(0L, mTestObject.waitForLongArray()[0]);
        executeJavaScript("testObject.setLongArray([false]);");
        assertEquals(0L, mTestObject.waitForLongArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should be 1.0.
        executeJavaScript("testObject.setFloatArray([true]);");
        assertEquals(0.0f, mTestObject.waitForFloatArray()[0]);
        executeJavaScript("testObject.setFloatArray([false]);");
        assertEquals(0.0f, mTestObject.waitForFloatArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should be 1.0.
        executeJavaScript("testObject.setDoubleArray([true]);");
        assertEquals(0.0, mTestObject.waitForDoubleArray()[0]);
        executeJavaScript("testObject.setDoubleArray([false]);");
        assertEquals(0.0, mTestObject.waitForDoubleArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should create array and create instances of java.lang.Number.
        executeJavaScript("testObject.setObjectArray([true]);");
        assertNull(mTestObject.waitForObjectArray());

        // LIVECONNECT_COMPLIANCE: Should create instances of java.lang.String.
        executeJavaScript("testObject.setStringArray([true]);");
        assertNull(mTestObject.waitForStringArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        executeJavaScript("testObject.setCustomTypeArray([true]);");
        assertNull(mTestObject.waitForCustomTypeArray());
    }

    // Test passing an array of JavaScript strings to a method which takes a
    // Java array.
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testPassString() throws Throwable {
        // LIVECONNECT_COMPLIANCE: Non-empty string should convert to true.
        executeJavaScript("testObject.setBooleanArray([\"+042.10\"]);");
        assertFalse(mTestObject.waitForBooleanArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should use valueOf() of appropriate type.
        executeJavaScript("testObject.setByteArray([\"+042.10\"]);");
        assertEquals(0, mTestObject.waitForByteArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should decode and convert to numeric char value.
        executeJavaScript("testObject.setCharArray([\"+042.10\"]);");
        assertEquals(0, mTestObject.waitForCharArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should use valueOf() of appropriate type.
        executeJavaScript("testObject.setShortArray([\"+042.10\"]);");
        assertEquals(0, mTestObject.waitForShortArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should use valueOf() of appropriate type.
        executeJavaScript("testObject.setIntArray([\"+042.10\"]);");
        assertEquals(0, mTestObject.waitForIntArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should use valueOf() of appropriate type.
        executeJavaScript("testObject.setLongArray([\"+042.10\"]);");
        assertEquals(0L, mTestObject.waitForLongArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should use valueOf() of appropriate type.
        executeJavaScript("testObject.setFloatArray([\"+042.10\"]);");
        assertEquals(0.0f, mTestObject.waitForFloatArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should use valueOf() of appropriate type.
        executeJavaScript("testObject.setDoubleArray([\"+042.10\"]);");
        assertEquals(0.0, mTestObject.waitForDoubleArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should create array and create instances of java.lang.Number.
        executeJavaScript("testObject.setObjectArray([\"+042.10\"]);");
        assertNull(mTestObject.waitForObjectArray());

        executeJavaScript("testObject.setStringArray([\"+042.10\"]);");
        assertEquals("+042.10", mTestObject.waitForStringArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        executeJavaScript("testObject.setCustomTypeArray([\"+042.10\"]);");
        assertNull(mTestObject.waitForCustomTypeArray());
    }

    // Test passing an array of JavaScript objects to a method which takes a
    // Java array.
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testPassJavaScriptObject() throws Throwable {
        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        executeJavaScript("testObject.setBooleanArray([{foo: 42}]);");
        assertFalse(mTestObject.waitForBooleanArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        executeJavaScript("testObject.setByteArray([{foo: 42}]);");
        assertEquals(0, mTestObject.waitForByteArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        executeJavaScript("testObject.setCharArray([{foo: 42}]);");
        assertEquals('\u0000', mTestObject.waitForCharArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        executeJavaScript("testObject.setShortArray([{foo: 42}]);");
        assertEquals(0, mTestObject.waitForShortArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        executeJavaScript("testObject.setIntArray([{foo: 42}]);");
        assertEquals(0, mTestObject.waitForIntArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        executeJavaScript("testObject.setLongArray([{foo: 42}]);");
        assertEquals(0L, mTestObject.waitForLongArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        executeJavaScript("testObject.setFloatArray([{foo: 42}]);");
        assertEquals(0.0f, mTestObject.waitForFloatArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        executeJavaScript("testObject.setDoubleArray([{foo: 42}]);");
        assertEquals(0.0, mTestObject.waitForDoubleArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        executeJavaScript("testObject.setObjectArray([{foo: 42}]);");
        assertNull(mTestObject.waitForObjectArray());

        // LIVECONNECT_COMPLIANCE: Should call toString() on object.
        executeJavaScript("testObject.setStringArray([{foo: 42}]);");
        assertNull(mTestObject.waitForStringArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        executeJavaScript("testObject.setCustomTypeArray([{foo: 42}]);");
        assertNull(mTestObject.waitForCustomTypeArray());
    }

    // Test passing an array of Java objects to a method which takes a Java
    // array.
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testPassJavaObject() throws Throwable {
        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        executeJavaScript("testObject.setBooleanArray([testObject.getObjectInstance()]);");
        assertFalse(mTestObject.waitForBooleanArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        executeJavaScript("testObject.setByteArray([testObject.getObjectInstance()]);");
        assertEquals(0, mTestObject.waitForByteArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        executeJavaScript("testObject.setCharArray([testObject.getObjectInstance()]);");
        assertEquals('\u0000', mTestObject.waitForCharArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        executeJavaScript("testObject.setShortArray([testObject.getObjectInstance()]);");
        assertEquals(0, mTestObject.waitForShortArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        executeJavaScript("testObject.setIntArray([testObject.getObjectInstance()]);");
        assertEquals(0, mTestObject.waitForIntArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        executeJavaScript("testObject.setLongArray([testObject.getObjectInstance()]);");
        assertEquals(0L, mTestObject.waitForLongArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        executeJavaScript("testObject.setFloatArray([testObject.getObjectInstance()]);");
        assertEquals(0.0f, mTestObject.waitForFloatArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should raise a JavaScript exception.
        executeJavaScript("testObject.setDoubleArray([testObject.getObjectInstance()]);");
        assertEquals(0.0, mTestObject.waitForDoubleArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should create an array and pass Java object.
        executeJavaScript("testObject.setObjectArray([testObject.getObjectInstance()]);");
        assertNull(mTestObject.waitForObjectArray());

        // LIVECONNECT_COMPLIANCE: Should call toString() on object.
        executeJavaScript("testObject.setStringArray([testObject.getObjectInstance()]);");
        assertNull(mTestObject.waitForStringArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should create array and pass Java object.
        executeJavaScript("testObject.setCustomTypeArray([testObject.getObjectInstance()]);");
        assertNull(mTestObject.waitForCustomTypeArray());
        executeJavaScript("testObject.setCustomTypeArray([testObject.getCustomTypeInstance()]);");
        assertNull(mTestObject.waitForCustomTypeArray());
    }

    // Test passing an array of JavaScript null values to a method which takes
    // a Java array.
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testPassNull() throws Throwable {
        executeJavaScript("testObject.setByteArray([null]);");
        assertEquals(0, mTestObject.waitForByteArray()[0]);

        executeJavaScript("testObject.setCharArray([null]);");
        assertEquals('\u0000', mTestObject.waitForCharArray()[0]);

        executeJavaScript("testObject.setShortArray([null]);");
        assertEquals(0, mTestObject.waitForShortArray()[0]);

        executeJavaScript("testObject.setIntArray([null]);");
        assertEquals(0, mTestObject.waitForIntArray()[0]);

        executeJavaScript("testObject.setLongArray([null]);");
        assertEquals(0L, mTestObject.waitForLongArray()[0]);

        executeJavaScript("testObject.setFloatArray([null]);");
        assertEquals(0.0f, mTestObject.waitForFloatArray()[0]);

        executeJavaScript("testObject.setDoubleArray([null]);");
        assertEquals(0.0, mTestObject.waitForDoubleArray()[0]);

        executeJavaScript("testObject.setBooleanArray([null]);");
        assertFalse(mTestObject.waitForBooleanArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should create array and pass null.
        executeJavaScript("testObject.setObjectArray([null]);");
        assertNull(mTestObject.waitForObjectArray());

        executeJavaScript("testObject.setStringArray([null]);");
        assertNull(mTestObject.waitForStringArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should create array and pass null.
        executeJavaScript("testObject.setCustomTypeArray([null]);");
        assertNull(mTestObject.waitForCustomTypeArray());
    }

    // Test passing an array of JavaScript undefined values to a method which
    // takes a Java array.
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testPassUndefined() throws Throwable {
        executeJavaScript("testObject.setByteArray([undefined]);");
        assertEquals(0, mTestObject.waitForByteArray()[0]);

        executeJavaScript("testObject.setCharArray([undefined]);");
        assertEquals(0, mTestObject.waitForCharArray()[0]);

        executeJavaScript("testObject.setShortArray([undefined]);");
        assertEquals(0, mTestObject.waitForShortArray()[0]);

        executeJavaScript("testObject.setIntArray([undefined]);");
        assertEquals(0, mTestObject.waitForIntArray()[0]);

        executeJavaScript("testObject.setLongArray([undefined]);");
        assertEquals(0L, mTestObject.waitForLongArray()[0]);

        executeJavaScript("testObject.setFloatArray([undefined]);");
        assertEquals(0.0f, mTestObject.waitForFloatArray()[0]);

        executeJavaScript("testObject.setDoubleArray([undefined]);");
        assertEquals(0.0, mTestObject.waitForDoubleArray()[0]);

        executeJavaScript("testObject.setBooleanArray([undefined]);");
        assertEquals(false, mTestObject.waitForBooleanArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should create array and pass null.
        executeJavaScript("testObject.setObjectArray([undefined]);");
        assertNull(mTestObject.waitForObjectArray());

        executeJavaScript("testObject.setStringArray([undefined]);");
        assertNull(mTestObject.waitForStringArray()[0]);

        // LIVECONNECT_COMPLIANCE: Should create array and pass null.
        executeJavaScript("testObject.setCustomTypeArray([undefined]);");
        assertNull(mTestObject.waitForCustomTypeArray());
    }

    // Test passing a typed Int8Array to a method which takes a Java array.
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testPassInt8Array() throws Throwable {
        executeJavaScript("buffer = new ArrayBuffer(1);");
        executeJavaScript("int8_array = new Int8Array(buffer);");
        executeJavaScript("int8_array[0] = 42;");

        executeJavaScript("testObject.setBooleanArray(int8_array);");
        assertFalse(mTestObject.waitForBooleanArray()[0]);

        executeJavaScript("testObject.setByteArray(int8_array);");
        assertEquals(42, mTestObject.waitForByteArray()[0]);

        executeJavaScript("testObject.setCharArray(int8_array);");
        assertEquals(42, mTestObject.waitForCharArray()[0]);

        executeJavaScript("testObject.setShortArray(int8_array);");
        assertEquals(42, mTestObject.waitForShortArray()[0]);

        executeJavaScript("testObject.setIntArray(int8_array);");
        assertEquals(42, mTestObject.waitForIntArray()[0]);

        executeJavaScript("testObject.setLongArray(int8_array);");
        assertEquals(42L, mTestObject.waitForLongArray()[0]);

        executeJavaScript("testObject.setFloatArray(int8_array);");
        assertEquals(42.0f, mTestObject.waitForFloatArray()[0]);

        executeJavaScript("testObject.setDoubleArray(int8_array);");
        assertEquals(42.0, mTestObject.waitForDoubleArray()[0]);

        executeJavaScript("testObject.setObjectArray(int8_array);");
        assertNull(mTestObject.waitForObjectArray());

        executeJavaScript("testObject.setStringArray(int8_array);");
        assertNull(mTestObject.waitForStringArray()[0]);

        executeJavaScript("testObject.setCustomTypeArray(int8_array);");
        assertNull(mTestObject.waitForCustomTypeArray());
    }

    // Test passing a typed Uint8Array to a method which takes a Java array.
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testPassUint8Array() throws Throwable {
        executeJavaScript("buffer = new ArrayBuffer(1);");
        executeJavaScript("uint8_array = new Uint8Array(buffer);");
        executeJavaScript("uint8_array[0] = 42;");

        executeJavaScript("testObject.setBooleanArray(uint8_array);");
        assertFalse(mTestObject.waitForBooleanArray()[0]);

        executeJavaScript("testObject.setByteArray(uint8_array);");
        assertEquals(42, mTestObject.waitForByteArray()[0]);

        executeJavaScript("testObject.setCharArray(uint8_array);");
        assertEquals(42, mTestObject.waitForCharArray()[0]);

        executeJavaScript("testObject.setShortArray(uint8_array);");
        assertEquals(42, mTestObject.waitForShortArray()[0]);

        executeJavaScript("testObject.setIntArray(uint8_array);");
        assertEquals(42, mTestObject.waitForIntArray()[0]);

        executeJavaScript("testObject.setLongArray(uint8_array);");
        assertEquals(42L, mTestObject.waitForLongArray()[0]);

        executeJavaScript("testObject.setFloatArray(uint8_array);");
        assertEquals(42.0f, mTestObject.waitForFloatArray()[0]);

        executeJavaScript("testObject.setDoubleArray(uint8_array);");
        assertEquals(42.0, mTestObject.waitForDoubleArray()[0]);

        executeJavaScript("testObject.setObjectArray(uint8_array);");
        assertNull(mTestObject.waitForObjectArray());

        executeJavaScript("testObject.setStringArray(uint8_array);");
        assertNull(mTestObject.waitForStringArray()[0]);

        executeJavaScript("testObject.setCustomTypeArray(uint8_array);");
        assertNull(mTestObject.waitForCustomTypeArray());
    }

    // Test passing a typed Int16Array to a method which takes a Java array.
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testPassInt16Array() throws Throwable {
        executeJavaScript("buffer = new ArrayBuffer(2);");
        executeJavaScript("int16_array = new Int16Array(buffer);");
        executeJavaScript("int16_array[0] = 42;");

        executeJavaScript("testObject.setBooleanArray(int16_array);");
        assertFalse(mTestObject.waitForBooleanArray()[0]);

        executeJavaScript("testObject.setByteArray(int16_array);");
        assertEquals(42, mTestObject.waitForByteArray()[0]);

        executeJavaScript("testObject.setCharArray(int16_array);");
        assertEquals(42, mTestObject.waitForCharArray()[0]);

        executeJavaScript("testObject.setShortArray(int16_array);");
        assertEquals(42, mTestObject.waitForShortArray()[0]);

        executeJavaScript("testObject.setIntArray(int16_array);");
        assertEquals(42, mTestObject.waitForIntArray()[0]);

        executeJavaScript("testObject.setLongArray(int16_array);");
        assertEquals(42L, mTestObject.waitForLongArray()[0]);

        executeJavaScript("testObject.setFloatArray(int16_array);");
        assertEquals(42.0f, mTestObject.waitForFloatArray()[0]);

        executeJavaScript("testObject.setDoubleArray(int16_array);");
        assertEquals(42.0, mTestObject.waitForDoubleArray()[0]);

        executeJavaScript("testObject.setObjectArray(int16_array);");
        assertNull(mTestObject.waitForObjectArray());

        executeJavaScript("testObject.setStringArray(int16_array);");
        assertNull(mTestObject.waitForStringArray()[0]);

        executeJavaScript("testObject.setCustomTypeArray(int16_array);");
        assertNull(mTestObject.waitForCustomTypeArray());
    }

    // Test passing a typed Uint16Array to a method which takes a Java array.
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testPassUint16Array() throws Throwable {
        executeJavaScript("buffer = new ArrayBuffer(2);");
        executeJavaScript("uint16_array = new Uint16Array(buffer);");
        executeJavaScript("uint16_array[0] = 42;");

        executeJavaScript("testObject.setBooleanArray(uint16_array);");
        assertFalse(mTestObject.waitForBooleanArray()[0]);

        executeJavaScript("testObject.setByteArray(uint16_array);");
        assertEquals(42, mTestObject.waitForByteArray()[0]);

        executeJavaScript("testObject.setCharArray(uint16_array);");
        assertEquals(42, mTestObject.waitForCharArray()[0]);

        executeJavaScript("testObject.setShortArray(uint16_array);");
        assertEquals(42, mTestObject.waitForShortArray()[0]);

        executeJavaScript("testObject.setIntArray(uint16_array);");
        assertEquals(42, mTestObject.waitForIntArray()[0]);

        executeJavaScript("testObject.setLongArray(uint16_array);");
        assertEquals(42L, mTestObject.waitForLongArray()[0]);

        executeJavaScript("testObject.setFloatArray(uint16_array);");
        assertEquals(42.0f, mTestObject.waitForFloatArray()[0]);

        executeJavaScript("testObject.setDoubleArray(uint16_array);");
        assertEquals(42.0, mTestObject.waitForDoubleArray()[0]);

        executeJavaScript("testObject.setObjectArray(uint16_array);");
        assertNull(mTestObject.waitForObjectArray());

        executeJavaScript("testObject.setStringArray(uint16_array);");
        assertNull(mTestObject.waitForStringArray()[0]);

        executeJavaScript("testObject.setCustomTypeArray(uint16_array);");
        assertNull(mTestObject.waitForCustomTypeArray());
    }

    // Test passing a typed Int32Array to a method which takes a Java array.
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testPassInt32Array() throws Throwable {
        executeJavaScript("buffer = new ArrayBuffer(4);");
        executeJavaScript("int32_array = new Int32Array(buffer);");
        executeJavaScript("int32_array[0] = 42;");

        executeJavaScript("testObject.setBooleanArray(int32_array);");
        assertFalse(mTestObject.waitForBooleanArray()[0]);

        executeJavaScript("testObject.setByteArray(int32_array);");
        assertEquals(42, mTestObject.waitForByteArray()[0]);

        executeJavaScript("testObject.setCharArray(int32_array);");
        assertEquals(42, mTestObject.waitForCharArray()[0]);

        executeJavaScript("testObject.setShortArray(int32_array);");
        assertEquals(42, mTestObject.waitForShortArray()[0]);

        executeJavaScript("testObject.setIntArray(int32_array);");
        assertEquals(42, mTestObject.waitForIntArray()[0]);

        executeJavaScript("testObject.setLongArray(int32_array);");
        assertEquals(42L, mTestObject.waitForLongArray()[0]);

        executeJavaScript("testObject.setFloatArray(int32_array);");
        assertEquals(42.0f, mTestObject.waitForFloatArray()[0]);

        executeJavaScript("testObject.setDoubleArray(int32_array);");
        assertEquals(42.0, mTestObject.waitForDoubleArray()[0]);

        executeJavaScript("testObject.setObjectArray(int32_array);");
        assertNull(mTestObject.waitForObjectArray());

        executeJavaScript("testObject.setStringArray(int32_array);");
        assertNull(mTestObject.waitForStringArray()[0]);

        executeJavaScript("testObject.setCustomTypeArray(int32_array);");
        assertNull(mTestObject.waitForCustomTypeArray());
    }

    // Test passing a typed Uint32Array to a method which takes a Java array.
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testPassUint32Array() throws Throwable {
        executeJavaScript("buffer = new ArrayBuffer(4);");
        executeJavaScript("uint32_array = new Uint32Array(buffer);");
        executeJavaScript("uint32_array[0] = 42;");

        executeJavaScript("testObject.setBooleanArray(uint32_array);");
        assertFalse(mTestObject.waitForBooleanArray()[0]);

        executeJavaScript("testObject.setByteArray(uint32_array);");
        assertEquals(42, mTestObject.waitForByteArray()[0]);

        executeJavaScript("testObject.setCharArray(uint32_array);");
        assertEquals(42, mTestObject.waitForCharArray()[0]);

        executeJavaScript("testObject.setShortArray(uint32_array);");
        assertEquals(42, mTestObject.waitForShortArray()[0]);

        executeJavaScript("testObject.setIntArray(uint32_array);");
        assertEquals(42, mTestObject.waitForIntArray()[0]);

        executeJavaScript("testObject.setLongArray(uint32_array);");
        assertEquals(42L, mTestObject.waitForLongArray()[0]);

        executeJavaScript("testObject.setFloatArray(uint32_array);");
        assertEquals(42.0f, mTestObject.waitForFloatArray()[0]);

        executeJavaScript("testObject.setDoubleArray(uint32_array);");
        assertEquals(42.0, mTestObject.waitForDoubleArray()[0]);

        executeJavaScript("testObject.setObjectArray(uint32_array);");
        assertNull(mTestObject.waitForObjectArray());

        executeJavaScript("testObject.setStringArray(uint32_array);");
        assertNull(mTestObject.waitForStringArray()[0]);

        executeJavaScript("testObject.setCustomTypeArray(uint32_array);");
        assertNull(mTestObject.waitForCustomTypeArray());
    }

    // Test passing a typed Float32Array to a method which takes a Java array.
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testPassFloat32Array() throws Throwable {
        executeJavaScript("buffer = new ArrayBuffer(4);");
        executeJavaScript("float32_array = new Float32Array(buffer);");
        executeJavaScript("float32_array[0] = 42.0;");

        executeJavaScript("testObject.setBooleanArray(float32_array);");
        assertFalse(mTestObject.waitForBooleanArray()[0]);

        executeJavaScript("testObject.setByteArray(float32_array);");
        assertEquals(42, mTestObject.waitForByteArray()[0]);

        executeJavaScript("testObject.setCharArray(float32_array);");
        assertEquals('\u0000', mTestObject.waitForCharArray()[0]);

        executeJavaScript("testObject.setShortArray(float32_array);");
        assertEquals(42, mTestObject.waitForShortArray()[0]);

        executeJavaScript("testObject.setIntArray(float32_array);");
        assertEquals(42, mTestObject.waitForIntArray()[0]);

        executeJavaScript("testObject.setLongArray(float32_array);");
        assertEquals(42L, mTestObject.waitForLongArray()[0]);

        executeJavaScript("testObject.setFloatArray(float32_array);");
        assertEquals(42.0f, mTestObject.waitForFloatArray()[0]);

        executeJavaScript("testObject.setDoubleArray(float32_array);");
        assertEquals(42.0, mTestObject.waitForDoubleArray()[0]);

        executeJavaScript("testObject.setObjectArray(float32_array);");
        assertNull(mTestObject.waitForObjectArray());

        executeJavaScript("testObject.setStringArray(float32_array);");
        assertNull(mTestObject.waitForStringArray()[0]);

        executeJavaScript("testObject.setCustomTypeArray(float32_array);");
        assertNull(mTestObject.waitForCustomTypeArray());
    }

    // Test passing a typed Float64Array to a method which takes a Java array.
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testPassFloat64Array() throws Throwable {
        executeJavaScript("buffer = new ArrayBuffer(8);");
        executeJavaScript("float64_array = new Float64Array(buffer);");
        executeJavaScript("float64_array[0] = 42.0;");

        executeJavaScript("testObject.setBooleanArray(float64_array);");
        assertFalse(mTestObject.waitForBooleanArray()[0]);

        executeJavaScript("testObject.setByteArray(float64_array);");
        assertEquals(42, mTestObject.waitForByteArray()[0]);

        executeJavaScript("testObject.setCharArray(float64_array);");
        assertEquals('\u0000', mTestObject.waitForCharArray()[0]);

        executeJavaScript("testObject.setShortArray(float64_array);");
        assertEquals(42, mTestObject.waitForShortArray()[0]);

        executeJavaScript("testObject.setIntArray(float64_array);");
        assertEquals(42, mTestObject.waitForIntArray()[0]);

        executeJavaScript("testObject.setLongArray(float64_array);");
        assertEquals(42L, mTestObject.waitForLongArray()[0]);

        executeJavaScript("testObject.setFloatArray(float64_array);");
        assertEquals(42.0f, mTestObject.waitForFloatArray()[0]);

        executeJavaScript("testObject.setDoubleArray(float64_array);");
        assertEquals(42.0, mTestObject.waitForDoubleArray()[0]);

        executeJavaScript("testObject.setObjectArray(float64_array);");
        assertNull(mTestObject.waitForObjectArray());

        executeJavaScript("testObject.setStringArray(float64_array);");
        assertNull(mTestObject.waitForStringArray()[0]);

        executeJavaScript("testObject.setCustomTypeArray(float64_array);");
        assertNull(mTestObject.waitForCustomTypeArray());
    }
}
