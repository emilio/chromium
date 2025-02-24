// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.Configuration;
import android.graphics.Canvas;
import android.graphics.Point;
import android.graphics.Rect;
import android.support.annotation.Nullable;
import android.support.v7.widget.DefaultItemAnimator;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.RecyclerView.AdapterDataObserver;
import android.support.v7.widget.RecyclerView.ViewHolder;
import android.text.Editable;
import android.text.TextWatcher;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnLayoutChangeListener;
import android.view.ViewStub;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.base.TraceEvent;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ntp.LogoBridge.Logo;
import org.chromium.chrome.browser.ntp.LogoBridge.LogoObserver;
import org.chromium.chrome.browser.ntp.NewTabPage.OnSearchBoxScrollListener;
import org.chromium.chrome.browser.ntp.cards.CardsVariationParameters;
import org.chromium.chrome.browser.ntp.cards.NewTabPageAdapter;
import org.chromium.chrome.browser.ntp.cards.NewTabPageRecyclerView;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.suggestions.DestructionObserver;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegate;
import org.chromium.chrome.browser.suggestions.Tile;
import org.chromium.chrome.browser.suggestions.TileGridLayout;
import org.chromium.chrome.browser.suggestions.TileGroup;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.browser.util.MathUtils;
import org.chromium.chrome.browser.util.ViewUtils;
import org.chromium.chrome.browser.widget.displaystyle.UiConfig;
import org.chromium.ui.base.DeviceFormFactor;

/**
 * The native new tab page, represented by some basic data such as title and url, and an Android
 * View that displays the page.
 */
public class NewTabPageView
        extends FrameLayout implements OnLayoutChangeListener, TileGroup.Observer {
    private static final String TAG = "NewTabPageView";

    private static final long SNAP_SCROLL_DELAY_MS = 30;

    /**
     * Experiment parameter for the maximum number of tile suggestion rows to show.
     */
    private static final String PARAM_NTP_MAX_TILE_ROWS = "ntp_max_tile_rows";

    /**
     * Experiment parameter for the number of tile title lines to show.
     */
    private static final String PARAM_NTP_TILE_TITLE_LINES = "ntp_tile_title_lines";

    /**
     * Experiment parameter for whether to use the condensed tile layout on small screens.
     */
    private static final String PARAM_CONDENSED_TILE_LAYOUT_FOR_SMALL_SCREENS_ENABLED =
            "condensed_tile_layout_for_small_screens_enabled";

    /**
     * Experiment parameter for whether to use the condensed tile layout on large screens.
     */
    private static final String PARAM_CONDENSED_TILE_LAYOUT_FOR_LARGE_SCREENS_ENABLED =
            "condensed_tile_layout_for_large_screens_enabled";

    private NewTabPageRecyclerView mRecyclerView;

    private NewTabPageLayout mNewTabPageLayout;
    private LogoView mSearchProviderLogoView;
    private View mSearchBoxView;
    private ImageView mVoiceSearchButton;
    private TileGridLayout mTileGridLayout;
    private View mTileGridPlaceholder;
    private View mNoSearchLogoSpacer;

    private OnSearchBoxScrollListener mSearchBoxScrollListener;

    private ChromeActivity mActivity;
    private NewTabPageManager mManager;
    private LogoView.Delegate mLogoDelegate;
    private TileGroup.Delegate mTileGroupDelegate;
    private TileGroup mTileGroup;
    private UiConfig mUiConfig;
    private boolean mFirstShow = true;
    private boolean mSearchProviderHasLogo = true;
    private boolean mPendingSnapScroll;
    private boolean mInitialized;

    /**
     * The number of asynchronous tasks that need to complete before the page is done loading.
     * This starts at one to track when the view is finished attaching to the window.
     */
    private int mPendingLoadTasks = 1;
    private boolean mLoadHasCompleted;

    private float mUrlFocusChangePercent;
    private boolean mDisableUrlFocusChangeAnimations;

    /** Flag used to request some layout changes after the next layout pass is completed. */
    private boolean mTileCountChanged;
    private boolean mSnapshotTileGridChanged;
    private boolean mNewTabPageRecyclerViewChanged;
    private int mSnapshotWidth;
    private int mSnapshotHeight;
    private int mSnapshotScrollY;
    private ContextMenuManager mContextMenuManager;

    /**
     * Manages the view interaction with the rest of the system.
     */
    public interface NewTabPageManager extends SuggestionsUiDelegate {
        /** @return Whether the location bar is shown in the NTP. */
        boolean isLocationBarShownInNTP();

        /** @return Whether voice search is enabled and the microphone should be shown. */
        boolean isVoiceSearchEnabled();

        /** @return Whether the omnibox 'Search or type URL' text should be shown. */
        boolean isFakeOmniboxTextEnabledTablet();

        /**
         * Animates the search box up into the omnibox and bring up the keyboard.
         * @param beginVoiceSearch Whether to begin a voice search.
         * @param pastedText Text to paste in the omnibox after it's been focused. May be null.
         */
        void focusSearchBox(boolean beginVoiceSearch, String pastedText);

        /**
         * @return whether the {@link NewTabPage} associated with this manager is the current page
         * displayed to the user.
         */
        boolean isCurrentPage();

        /**
         * @return The context menu manager. Will be {@code null} if the {@link NewTabPageView} is
         * not done initialising.
         */
        @Nullable
        ContextMenuManager getContextMenuManager();
    }

    /**
     * Default constructor required for XML inflation.
     */
    public NewTabPageView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Initializes the NTP. This must be called immediately after inflation, before this object is
     * used in any other way.
     *
     * @param manager NewTabPageManager used to perform various actions when the user interacts
     *                with the page.
     * @param tab The Tab that is showing this new tab page.
     * @param searchProviderHasLogo Whether the search provider has a logo.
     * @param scrollPosition The adapter scroll position to initialize to.
     */
    public void initialize(NewTabPageManager manager, Tab tab, TileGroup.Delegate tileGroupDelegate,
            boolean searchProviderHasLogo, int scrollPosition) {
        TraceEvent.begin(TAG + ".initialize()");
        mActivity = tab.getActivity();
        mManager = manager;
        mTileGroupDelegate = tileGroupDelegate;
        mUiConfig = new UiConfig(this);

        assert manager.getSuggestionsSource() != null;

        mRecyclerView = new NewTabPageRecyclerView(getContext());
        mRecyclerView.setContainsLocationBar(manager.isLocationBarShownInNTP());
        addView(mRecyclerView);

        // Don't attach now, the recyclerView itself will determine when to do it.
        mNewTabPageLayout = mRecyclerView.getAboveTheFoldView();

        mRecyclerView.setItemAnimator(new DefaultItemAnimator() {
            @Override
            public void onAnimationFinished(ViewHolder viewHolder) {
                super.onAnimationFinished(viewHolder);
                // When removing sections, because the animations are all translations, the
                // scroll events don't fire and we can get in the situation where the toolbar
                // buttons disappear.
                updateSearchBoxOnScroll();
            }
        });

        mContextMenuManager =
                new ContextMenuManager(mActivity, mManager.getNavigationDelegate(), mRecyclerView);
        mActivity.getWindowAndroid().addContextMenuCloseListener(mContextMenuManager);
        manager.addDestructionObserver(new DestructionObserver() {
            @Override
            public void onDestroy() {
                mActivity.getWindowAndroid().removeContextMenuCloseListener(mContextMenuManager);
            }
        });

        OfflinePageBridge offlinePageBridge =
                OfflinePageBridge.getForProfile(Profile.getLastUsedProfile());

        mTileGridLayout = (TileGridLayout) mNewTabPageLayout.findViewById(R.id.tile_grid_layout);
        mTileGridLayout.setMaxRows(getMaxTileRows(searchProviderHasLogo));
        mTileGridLayout.setMaxColumns(getMaxTileColumns());
        mTileGroup = new TileGroup(mActivity, mManager, mContextMenuManager, mTileGroupDelegate,
                /* observer = */ this, offlinePageBridge, getTileTitleLines());

        mSearchProviderLogoView =
                (LogoView) mNewTabPageLayout.findViewById(R.id.search_provider_logo);
        mLogoDelegate = new LogoDelegateImpl(tab, mSearchProviderLogoView);
        mSearchBoxView = mNewTabPageLayout.findViewById(R.id.search_box);
        mNoSearchLogoSpacer = mNewTabPageLayout.findViewById(R.id.no_search_logo_spacer);

        initializeSearchBoxTextView();
        initializeVoiceSearchButton();

        mNewTabPageLayout.addOnLayoutChangeListener(this);
        setSearchProviderHasLogo(searchProviderHasLogo);

        tab.addObserver(new EmptyTabObserver() {
            @Override
            public void onShown(Tab tab) {
                mTileGroup.onSwitchToForeground();
            }
        });
        mTileGroup.startObserving(getMaxTileRows(searchProviderHasLogo) * getMaxTileColumns());

        // Set up snippets
        NewTabPageAdapter newTabPageAdapter = new NewTabPageAdapter(mManager, mNewTabPageLayout,
                mUiConfig, offlinePageBridge, mContextMenuManager, /* tileGroupDelegate = */ null);
        newTabPageAdapter.refreshSuggestions();
        mRecyclerView.setAdapter(newTabPageAdapter);

        int scrollOffset;
        if (CardsVariationParameters.isScrollBelowTheFoldEnabled()) {
            scrollPosition = newTabPageAdapter.getFirstHeaderPosition();
            scrollOffset = getResources().getDimensionPixelSize(R.dimen.ntp_search_box_height);
        } else {
            scrollOffset = 0;
        }
        mRecyclerView.getLinearLayoutManager().scrollToPositionWithOffset(
                scrollPosition, scrollOffset);

        setupScrollHandling();

        // When the NewTabPageAdapter's data changes we need to invalidate any previous
        // screen captures of the NewTabPageView.
        newTabPageAdapter.registerAdapterDataObserver(new AdapterDataObserver() {
            @Override
            public void onChanged() {
                mNewTabPageRecyclerViewChanged = true;
            }

            @Override
            public void onItemRangeChanged(int positionStart, int itemCount) {
                onChanged();
            }

            @Override
            public void onItemRangeInserted(int positionStart, int itemCount) {
                onChanged();
            }

            @Override
            public void onItemRangeRemoved(int positionStart, int itemCount) {
                onChanged();
            }

            @Override
            public void onItemRangeMoved(int fromPosition, int toPosition, int itemCount) {
                onChanged();
            }
        });

        mInitialized = true;

        TraceEvent.end(TAG + ".initialize()");
    }

    /**
     * Sets up the hint text and event handlers for the search box text view.
     */
    private void initializeSearchBoxTextView() {
        TraceEvent.begin(TAG + ".initializeSearchBoxTextView()");
        final TextView searchBoxTextView = (TextView) mSearchBoxView
                .findViewById(R.id.search_box_text);

        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.NTP_SHOW_GOOGLE_G_IN_OMNIBOX)) {
            searchBoxTextView.setCompoundDrawablePadding(0);

            // Not using the relative version of this call because we only want to clear
            // the drawables.
            searchBoxTextView.setCompoundDrawables(null, null, null, null);
        }

        String hintText = getResources().getString(R.string.search_or_type_url);
        if (!DeviceFormFactor.isTablet(getContext()) || mManager.isFakeOmniboxTextEnabledTablet()) {
            searchBoxTextView.setHint(hintText);
        } else {
            searchBoxTextView.setContentDescription(hintText);
        }
        searchBoxTextView.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mManager.focusSearchBox(false, null);
            }
        });
        searchBoxTextView.addTextChangedListener(new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {
            }

            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {
            }

            @Override
            public void afterTextChanged(Editable s) {
                if (s.length() == 0) return;
                mManager.focusSearchBox(false, s.toString());
                searchBoxTextView.setText("");
            }
        });
        TraceEvent.end(TAG + ".initializeSearchBoxTextView()");
    }

    private void initializeVoiceSearchButton() {
        TraceEvent.begin(TAG + ".initializeVoiceSearchButton()");
        mVoiceSearchButton = (ImageView) mNewTabPageLayout.findViewById(R.id.voice_search_button);
        mVoiceSearchButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mManager.focusSearchBox(true, null);
            }
        });
        TraceEvent.end(TAG + ".initializeVoiceSearchButton()");
    }

    private void updateSearchBoxOnScroll() {
        if (mDisableUrlFocusChangeAnimations) return;

        // When the page changes (tab switching or new page loading), it is possible that events
        // (e.g. delayed RecyclerView change notifications) trigger calls to these methods after
        // the current page changes. We check it again to make sure we don't attempt to update the
        // wrong page.
        if (!mManager.isCurrentPage()) return;

        if (mSearchBoxScrollListener != null) {
            mSearchBoxScrollListener.onNtpScrollChanged(getToolbarTransitionPercentage());
        }
    }

    /**
     * Calculates the percentage (between 0 and 1) of the transition from the search box to the
     * omnibox at the top of the New Tab Page, which is determined by the amount of scrolling and
     * the position of the search box.
     *
     * @return the transition percentage
     */
    private float getToolbarTransitionPercentage() {
        // During startup the view may not be fully initialized, so we only calculate the current
        // percentage if some basic view properties (height of the containing view, position of the
        // search box) are sane.
        if (getRecyclerView().getHeight() == 0) return 0f;

        if (!mRecyclerView.isFirstItemVisible()) {
            // getVerticalScroll is valid only for the RecyclerView if the first item is visible.
            // If the first item is not visible, we must have scrolled quite far and we know the
            // toolbar transition should be 100%. This might be the initial scroll position due to
            // the scroll restore feature, so the search box will not have been laid out yet.
            return 1f;
        }

        int searchBoxTop = mSearchBoxView.getTop();
        if (searchBoxTop == 0) return 0f;

        // For all other calculations, add the search box padding, because it defines where the
        // visible "border" of the search box is.
        searchBoxTop += mSearchBoxView.getPaddingTop();

        final int scrollY = mRecyclerView.computeVerticalScrollOffset();
        final float transitionLength =
                getResources().getDimension(R.dimen.ntp_search_box_transition_length);
        // Tab strip height is zero on phones, nonzero on tablets.
        int tabStripHeight = getResources().getDimensionPixelSize(R.dimen.tab_strip_height);

        // |scrollY - searchBoxTop + tabStripHeight| gives the distance the search bar is from the
        // top of the tab.
        return MathUtils.clamp((scrollY - searchBoxTop + transitionLength + tabStripHeight)
                / transitionLength, 0f, 1f);
    }

    @VisibleForTesting
    public NewTabPageRecyclerView getRecyclerView() {
        return mRecyclerView;
    }

    /**
     * @return The placeholder that is shown above the fold when there is no other content to show,
     *         or null if it has not been inflated yet.
     */
    @VisibleForTesting
    @Nullable
    public View getPlaceholder() {
        return mTileGridPlaceholder;
    }

    @VisibleForTesting
    public TileGroup getTileGroup() {
        return mTileGroup;
    }

    /**
     * Adds listeners to scrolling to take care of snap scrolling and updating the search box on
     * scroll.
     */
    private void setupScrollHandling() {
        TraceEvent.begin(TAG + ".setupScrollHandling()");
        final Runnable mSnapScrollRunnable = new Runnable() {
            @Override
            public void run() {
                assert mPendingSnapScroll;
                mPendingSnapScroll = false;

                mRecyclerView.snapScroll(mSearchBoxView, getHeight());
            }
        };

        mRecyclerView.addOnScrollListener(new RecyclerView.OnScrollListener() {
            @Override
            public void onScrolled(RecyclerView recyclerView, int dx, int dy) {
                if (mPendingSnapScroll) {
                    mRecyclerView.removeCallbacks(mSnapScrollRunnable);
                    mRecyclerView.postDelayed(mSnapScrollRunnable, SNAP_SCROLL_DELAY_MS);
                }
                updateSearchBoxOnScroll();
                mRecyclerView.updatePeekingCardAndHeader();
            }
        });

        mRecyclerView.setOnTouchListener(new OnTouchListener() {
            @Override
            @SuppressLint("ClickableViewAccessibility")
            public boolean onTouch(View v, MotionEvent event) {
                mRecyclerView.removeCallbacks(mSnapScrollRunnable);

                if (event.getActionMasked() == MotionEvent.ACTION_CANCEL
                        || event.getActionMasked() == MotionEvent.ACTION_UP) {
                    mPendingSnapScroll = true;
                    mRecyclerView.postDelayed(mSnapScrollRunnable, SNAP_SCROLL_DELAY_MS);
                } else {
                    mPendingSnapScroll = false;
                }
                return false;
            }
        });
        TraceEvent.end(TAG + ".setupScrollHandling()");
    }

    /**
     * Decrements the count of pending load tasks and notifies the manager when the page load
     * is complete.
     */
    private void loadTaskCompleted() {
        assert mPendingLoadTasks > 0;
        mPendingLoadTasks--;
        if (mPendingLoadTasks == 0) {
            if (mLoadHasCompleted) {
                assert false;
            } else {
                mLoadHasCompleted = true;
                mTileGroupDelegate.onLoadingComplete(mTileGroup.getTiles());
                // Load the logo after everything else is finished, since it's lower priority.
                loadSearchProviderLogo();
            }
        }
    }

    /**
     * Loads the search provider logo (e.g. Google doodle), if any.
     */
    private void loadSearchProviderLogo() {
        mLogoDelegate.getSearchProviderLogo(new LogoObserver() {
            @Override
            public void onLogoAvailable(Logo logo, boolean fromCache) {
                if (logo == null && fromCache) return;
                mSearchProviderLogoView.setDelegate(mLogoDelegate);
                mSearchProviderLogoView.updateLogo(logo);
                mSnapshotTileGridChanged = true;
            }
        });
    }

    /**
     * Changes the layout depending on whether the selected search provider (e.g. Google, Bing)
     * has a logo.
     * @param hasLogo Whether the search provider has a logo.
     */
    public void setSearchProviderHasLogo(boolean hasLogo) {
        if (hasLogo == mSearchProviderHasLogo && mInitialized) return;
        mSearchProviderHasLogo = hasLogo;
        boolean showLogo = mSearchProviderHasLogo
                && !ChromeFeatureList.isEnabled(ChromeFeatureList.NTP_CONDENSED_LAYOUT);

        // Set a bit more top padding on the tile grid if there is no logo.
        int paddingTop = getResources().getDimensionPixelSize(showLogo
                        ? R.dimen.tile_grid_layout_padding_top
                        : R.dimen.tile_grid_layout_no_logo_padding_top);
        mTileGridLayout.setPadding(0, paddingTop, 0, mTileGridLayout.getPaddingBottom());

        // Hide or show the views above the tile grid as needed, including logo, search box, and
        // spacers.
        int visibility = mSearchProviderHasLogo ? View.VISIBLE : View.GONE;
        int logoVisibility = showLogo ? View.VISIBLE : View.GONE;
        int childCount = mNewTabPageLayout.getChildCount();
        for (int i = 0; i < childCount; i++) {
            View child = mNewTabPageLayout.getChildAt(i);
            if (child == mTileGridLayout) break;
            // Don't change the visibility of a ViewStub as that will automagically inflate it.
            if (child instanceof ViewStub) continue;
            if (child == mSearchProviderLogoView) {
                child.setVisibility(logoVisibility);
            } else {
                child.setVisibility(visibility);
            }
        }

        // Update snap scrolling for the fakebox.
        mRecyclerView.setContainsLocationBar(mManager.isLocationBarShownInNTP());

        updateTileGridPlaceholderVisibility();

        onUrlFocusAnimationChanged();

        mSnapshotTileGridChanged = true;
    }

    /**
     * Updates whether the NewTabPage should animate on URL focus changes.
     * @param disable Whether to disable the animations.
     */
    void setUrlFocusAnimationsDisabled(boolean disable) {
        if (disable == mDisableUrlFocusChangeAnimations) return;
        mDisableUrlFocusChangeAnimations = disable;
        if (!disable) onUrlFocusAnimationChanged();
    }

    /**
     * @return Whether URL focus animations are currently disabled.
     */
    boolean urlFocusAnimationsDisabled() {
        return mDisableUrlFocusChangeAnimations;
    }

    /**
     * Specifies the percentage the URL is focused during an animation.  1.0 specifies that the URL
     * bar has focus and has completed the focus animation.  0 is when the URL bar is does not have
     * any focus.
     *
     * @param percent The percentage of the URL bar focus animation.
     */
    void setUrlFocusChangeAnimationPercent(float percent) {
        mUrlFocusChangePercent = percent;
        onUrlFocusAnimationChanged();
    }

    /**
     * @return The percentage that the URL bar is focused during an animation.
     */
    @VisibleForTesting
    float getUrlFocusChangeAnimationPercent() {
        return mUrlFocusChangePercent;
    }

    private void onUrlFocusAnimationChanged() {
        if (mDisableUrlFocusChangeAnimations || FeatureUtilities.isChromeHomeEnabled()) return;

        // Translate so that the search box is at the top, but only upwards.
        float percent = mSearchProviderHasLogo ? mUrlFocusChangePercent : 0;
        int basePosition = mRecyclerView.computeVerticalScrollOffset()
                + mNewTabPageLayout.getPaddingTop();
        int target = Math.max(basePosition,
                    mSearchBoxView.getBottom() - mSearchBoxView.getPaddingBottom());

        mNewTabPageLayout.setTranslationY(percent * (basePosition - target));
    }

    /**
     * Updates the opacity of the search box when scrolling.
     *
     * @param alpha opacity (alpha) value to use.
     */
    public void setSearchBoxAlpha(float alpha) {
        mSearchBoxView.setAlpha(alpha);

        // Disable the search box contents if it is the process of being animated away.
        ViewUtils.setEnabledRecursive(mSearchBoxView, mSearchBoxView.getAlpha() == 1.0f);
    }

    /**
     * Updates the opacity of the search provider logo when scrolling.
     *
     * @param alpha opacity (alpha) value to use.
     */
    public void setSearchProviderLogoAlpha(float alpha) {
        mSearchProviderLogoView.setAlpha(alpha);
    }

    /**
     * Get the bounds of the search box in relation to the top level NewTabPage view.
     *
     * @param bounds The current drawing location of the search box.
     * @param translation The translation applied to the search box by the parent view hierarchy up
     *                    to the NewTabPage view.
     */
    void getSearchBoxBounds(Rect bounds, Point translation) {
        int searchBoxX = (int) mSearchBoxView.getX();
        int searchBoxY = (int) mSearchBoxView.getY();

        bounds.set(searchBoxX + mSearchBoxView.getPaddingLeft(),
                searchBoxY + mSearchBoxView.getPaddingTop(),
                searchBoxX + mSearchBoxView.getWidth() - mSearchBoxView.getPaddingRight(),
                searchBoxY + mSearchBoxView.getHeight() - mSearchBoxView.getPaddingBottom());

        translation.set(0, 0);

        View view = mSearchBoxView;
        while (true) {
            view = (View) view.getParent();
            if (view == null) {
                // The |mSearchBoxView| is not a child of this view. This can happen if the
                // RecyclerView detaches the NewTabPageLayout after it has been scrolled out of
                // view. Set the translation to the minimum Y value as an approximation.
                translation.y = Integer.MIN_VALUE;
                break;
            }
            translation.offset(-view.getScrollX(), -view.getScrollY());
            if (view == this) break;
            translation.offset((int) view.getX(), (int) view.getY());
        }
        bounds.offset(translation.x, translation.y);
    }

    /**
     * Sets the listener for search box scroll changes.
     * @param listener The listener to be notified on changes.
     */
    void setSearchBoxScrollListener(OnSearchBoxScrollListener listener) {
        mSearchBoxScrollListener = listener;
        if (mSearchBoxScrollListener != null) updateSearchBoxOnScroll();
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        assert mManager != null;

        if (mFirstShow) {
            loadTaskCompleted();
            mFirstShow = false;
            NewTabPageUma.recordSearchAvailableLoadTime(mActivity);
            TraceEvent.instant("NewTabPageSearchAvailable)");
        } else {
            // Trigger a scroll update when reattaching the window to signal the toolbar that
            // it needs to reset the NTP state.
            if (mManager.isLocationBarShownInNTP()) updateSearchBoxOnScroll();
        }
    }

    /**
     * Update the visibility of the voice search button based on whether the feature is currently
     * enabled.
     */
    void updateVoiceSearchButtonVisibility() {
        mVoiceSearchButton.setVisibility(mManager.isVoiceSearchEnabled() ? VISIBLE : GONE);
    }

    @Override
    protected void onWindowVisibilityChanged(int visibility) {
        super.onWindowVisibilityChanged(visibility);

        // On first run, the NewTabPageView is initialized behind the First Run Experience, meaning
        // the UiConfig will pickup the screen layout then. However onConfigurationChanged is not
        // called on orientation changes until the FRE is completed. This means that if a user
        // starts the FRE in one orientation, changes an orientation and then leaves the FRE the
        // UiConfig will have the wrong orientation. https://crbug.com/683886.
        mUiConfig.updateDisplayStyle();

        if (visibility == VISIBLE) {
            updateVoiceSearchButtonVisibility();
        }
    }

    /**
     * @see org.chromium.chrome.browser.compositor.layouts.content.
     *         InvalidationAwareThumbnailProvider#shouldCaptureThumbnail()
     */
    boolean shouldCaptureThumbnail() {
        if (getWidth() == 0 || getHeight() == 0) return false;

        return mNewTabPageRecyclerViewChanged || mSnapshotTileGridChanged
                || getWidth() != mSnapshotWidth || getHeight() != mSnapshotHeight
                || mRecyclerView.computeVerticalScrollOffset() != mSnapshotScrollY;
    }

    /**
     * @see org.chromium.chrome.browser.compositor.layouts.content.
     *         InvalidationAwareThumbnailProvider#captureThumbnail(Canvas)
     */
    void captureThumbnail(Canvas canvas) {
        mSearchProviderLogoView.endFadeAnimation();
        ViewUtils.captureBitmap(this, canvas);
        mSnapshotWidth = getWidth();
        mSnapshotHeight = getHeight();
        mSnapshotScrollY = mRecyclerView.computeVerticalScrollOffset();
        mSnapshotTileGridChanged = false;
        mNewTabPageRecyclerViewChanged = false;
    }

    // OnLayoutChangeListener overrides

    @Override
    public void onLayoutChange(View v, int left, int top, int right, int bottom,
            int oldLeft, int oldTop, int oldRight, int oldBottom) {
        int oldHeight = oldBottom - oldTop;
        int newHeight = bottom - top;

        if (oldHeight == newHeight && !mTileCountChanged) return;
        mTileCountChanged = false;

        // Re-apply the url focus change amount after a rotation to ensure the views are correctly
        // placed with their new layout configurations.
        onUrlFocusAnimationChanged();
        updateSearchBoxOnScroll();

        mRecyclerView.updatePeekingCardAndHeader();
        // The positioning of elements may have been changed (since the elements expand to fill
        // the available vertical space), so adjust the scroll.
        mRecyclerView.snapScroll(mSearchBoxView, getHeight());
    }

    /**
     * Shows the most visited placeholder ("Nothing to see here") if there are no most visited
     * items and there is no search provider logo.
     */
    private void updateTileGridPlaceholderVisibility() {
        boolean showPlaceholder = mTileGroup.hasReceivedData() && mTileGroup.getTiles().length == 0
                && !mSearchProviderHasLogo;

        mNoSearchLogoSpacer.setVisibility(
                (mSearchProviderHasLogo || showPlaceholder) ? View.GONE : View.INVISIBLE);

        if (showPlaceholder) {
            if (mTileGridPlaceholder == null) {
                ViewStub placeholderStub =
                        (ViewStub) mNewTabPageLayout.findViewById(R.id.tile_grid_placeholder_stub);

                mTileGridPlaceholder = placeholderStub.inflate();
            }
            mTileGridLayout.setVisibility(GONE);
            mTileGridPlaceholder.setVisibility(VISIBLE);
        } else if (mTileGridPlaceholder != null) {
            mTileGridLayout.setVisibility(VISIBLE);
            mTileGridPlaceholder.setVisibility(GONE);
        }
    }

    private static int getMaxTileRows(boolean searchProviderHasLogo) {
        int defaultValue = 2;
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.NTP_CONDENSED_LAYOUT)
                && !searchProviderHasLogo) {
            defaultValue = 3;
        }
        return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.NTP_CONDENSED_LAYOUT, PARAM_NTP_MAX_TILE_ROWS, defaultValue);
    }

    /**
     * Determines The maximum number of tiles to try and fit in a row. On smaller screens, there
     * may not be enough space to fit all of them.
     */
    private int getMaxTileColumns() {
        if (!mUiConfig.getCurrentDisplayStyle().isSmall()
                && ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                           ChromeFeatureList.NTP_CONDENSED_TILE_LAYOUT,
                           PARAM_CONDENSED_TILE_LAYOUT_FOR_LARGE_SCREENS_ENABLED, false)) {
            return 5;
        }
        return 4;
    }

    private static int getTileTitleLines() {
        int defaultValue = 2;
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.NTP_CONDENSED_LAYOUT)) {
            defaultValue = 1;
        }
        return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.NTP_CONDENSED_LAYOUT, PARAM_NTP_TILE_TITLE_LINES, defaultValue);
    }

    private boolean shouldUseCondensedTileLayout() {
        if (mUiConfig.getCurrentDisplayStyle().isSmall()) {
            return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                    ChromeFeatureList.NTP_CONDENSED_TILE_LAYOUT,
                    PARAM_CONDENSED_TILE_LAYOUT_FOR_SMALL_SCREENS_ENABLED, false);
        }
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.NTP_CONDENSED_TILE_LAYOUT,
                PARAM_CONDENSED_TILE_LAYOUT_FOR_LARGE_SCREENS_ENABLED, false);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        if (mNewTabPageLayout != null) {
            mNewTabPageLayout.setParentViewportHeight(MeasureSpec.getSize(heightMeasureSpec));
        }
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);

        mRecyclerView.updatePeekingCardAndHeader();
    }

    @Override
    protected void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);

        // When the viewport configuration changes, we want to update the display style so that the
        // observers are aware of the new available space. Another moment to do this update could
        // be through a OnLayoutChangeListener, but then we get notified of the change after the
        // layout pass, which means that the new style will only be visible after layout happens
        // again. We prefer updating here to avoid having to require that additional layout pass.
        mUiConfig.updateDisplayStyle();

        // Close the Context Menu as it may have moved (https://crbug.com/642688).
        mContextMenuManager.closeContextMenu();
    }

    /**
     * @return The adapter position the user has scrolled to.
     */
    public int getScrollPosition() {
        return mRecyclerView.getScrollPosition();
    }

    /** @return the context menu manager. */
    public ContextMenuManager getContextMenuManager() {
        return mContextMenuManager;
    }

    // TileGroup.Observer interface.

    @Override
    public void onTileDataChanged() {
        mTileGroup.renderTileViews(
                mTileGridLayout, !mLoadHasCompleted, shouldUseCondensedTileLayout());
        mSnapshotTileGridChanged = true;

        // The page contents are initially hidden; otherwise they'll be drawn centered on the page
        // before the tiles are available and then jump upwards to make space once the tiles are
        // available.
        if (mNewTabPageLayout.getVisibility() != View.VISIBLE) {
            mNewTabPageLayout.setVisibility(View.VISIBLE);
        }
    }

    @Override
    public void onTileCountChanged() {
        // If the number of tile rows change while the URL bar is focused, the icons'
        // position will be wrong. Schedule the translation to be updated.
        if (mUrlFocusChangePercent == 1f) mTileCountChanged = true;
        updateTileGridPlaceholderVisibility();
    }

    @Override
    public void onTileIconChanged(Tile tile) {
        mTileGridLayout.updateIconView(tile);
        mSnapshotTileGridChanged = true;
    }

    @Override
    public void onTileOfflineBadgeVisibilityChanged(Tile tile) {
        mTileGridLayout.updateOfflineBadge(tile);
        mSnapshotTileGridChanged = true;
    }

    @Override
    public void onLoadTaskAdded() {
        mPendingLoadTasks++;
    }

    @Override
    public void onLoadTaskCompleted() {
        loadTaskCompleted();
    }
}
