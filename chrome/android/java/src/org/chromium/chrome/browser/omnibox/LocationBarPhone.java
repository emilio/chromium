// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.support.v4.view.animation.FastOutLinearInInterpolator;
import android.text.Selection;
import android.util.AttributeSet;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.TouchDelegate;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.view.animation.Interpolator;
import android.widget.FrameLayout;
import android.widget.ImageView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.WindowDelegate;
import org.chromium.chrome.browser.appmenu.AppMenuButtonHelper;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.omaha.UpdateMenuItemHelper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.MathUtils;
import org.chromium.chrome.browser.widget.TintedImageButton;
import org.chromium.ui.UiUtils;

/**
 * A location bar implementation specific for smaller/phone screens.
 */
public class LocationBarPhone extends LocationBarLayout {
    private static final int KEYBOARD_MODE_CHANGE_DELAY_MS = 300;
    private static final int KEYBOARD_HIDE_DELAY_MS = 150;

    private static final int ACTION_BUTTON_TOUCH_OVERFLOW_LEFT = 15;

    private static final Interpolator GOOGLE_G_FADE_INTERPOLATOR =
            new FastOutLinearInInterpolator();

    private View mFirstVisibleFocusedView;
    private View mIncognitoBadge;
    private View mGoogleGContainer;
    private View mGoogleG;
    private View mUrlActionsContainer;
    private TintedImageButton mMenuButton;
    private ImageView mMenuBadge;
    private View mMenuButtonWrapper;
    private int mIncognitoBadgePadding;
    private int mGoogleGWidth;
    private int mGoogleGMargin;
    private float mUrlFocusChangePercent;
    private Runnable mKeyboardResizeModeTask;
    private ObjectAnimator mOmniboxBackgroundAnimator;

    private boolean mShowMenuBadge;
    private AnimatorSet mMenuBadgeAnimatorSet;
    private boolean mIsMenuBadgeAnimationRunning;

    /**
     * Constructor used to inflate from XML.
     */
    public LocationBarPhone(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mFirstVisibleFocusedView = findViewById(R.id.url_bar);
        mIncognitoBadge = findViewById(R.id.incognito_badge);
        mIncognitoBadgePadding =
                getResources().getDimensionPixelSize(R.dimen.location_bar_incognito_badge_padding);

        mGoogleGContainer = findViewById(R.id.google_g_container);
        mGoogleG = findViewById(R.id.google_g);
        mGoogleGWidth = getResources().getDimensionPixelSize(R.dimen.location_bar_google_g_width);
        mGoogleGMargin = getResources().getDimensionPixelSize(R.dimen.location_bar_google_g_margin);

        mUrlActionsContainer = findViewById(R.id.url_action_container);
        Rect delegateArea = new Rect();
        mUrlActionsContainer.getHitRect(delegateArea);
        delegateArea.left -= ACTION_BUTTON_TOUCH_OVERFLOW_LEFT;
        TouchDelegate touchDelegate = new TouchDelegate(delegateArea, mUrlActionsContainer);
        assert mUrlActionsContainer.getParent() == this;
        setTouchDelegate(touchDelegate);

        mMenuButton = (TintedImageButton) findViewById(R.id.document_menu_button);
        mMenuBadge = (ImageView) findViewById(R.id.document_menu_badge);
        mMenuButtonWrapper = findViewById(R.id.document_menu_button_wrapper);

        ((ViewGroup) mMenuButtonWrapper.getParent()).removeView(mMenuButtonWrapper);
    }

    @Override
    public void setMenuButtonHelper(final AppMenuButtonHelper helper) {
        super.setMenuButtonHelper(helper);
        mMenuButton.setOnTouchListener(new OnTouchListener() {
            @SuppressLint("ClickableViewAccessibility")
            @Override
            public boolean onTouch(View v, MotionEvent event) {
                return helper.onTouch(v, event);
            }
        });
        mMenuButton.setOnKeyListener(new OnKeyListener() {
            @Override
            public boolean onKey(View view, int keyCode, KeyEvent event) {
                if (keyCode == KeyEvent.KEYCODE_ENTER && event.getAction() == KeyEvent.ACTION_UP) {
                    return helper.onEnterKeyPress(view);
                }
                return false;
            }
        });
    }

    @Override
    public View getMenuAnchor() {
        return mMenuButton;
    }

    /**
     * @return The first view visible when the location bar is focused.
     */
    public View getFirstViewVisibleWhenFocused() {
        return mFirstVisibleFocusedView;
    }

    /**
     * Updates percentage of current the URL focus change animation.
     * @param percent 1.0 is 100% focused, 0 is completely unfocused.
     */
    public void setUrlFocusChangePercent(float percent) {
        mUrlFocusChangePercent = percent;

        if (percent > 0f) {
            mUrlActionsContainer.setVisibility(VISIBLE);
        } else if (percent == 0f && !isUrlFocusChangeInProgress()) {
            // If a URL focus change is in progress, then it will handle setting the visibility
            // correctly after it completes.  If done here, it would cause the URL to jump due
            // to a badly timed layout call.
            mUrlActionsContainer.setVisibility(GONE);
        }

        updateButtonVisibility();
    }

    @Override
    public void onUrlFocusChange(boolean hasFocus) {
        if (mOmniboxBackgroundAnimator != null && mOmniboxBackgroundAnimator.isRunning()) {
            mOmniboxBackgroundAnimator.cancel();
            mOmniboxBackgroundAnimator = null;
        }
        if (hasFocus) {
            // Remove the focus of this view once the URL field has taken focus as this view no
            // longer needs it.
            setFocusable(false);
            setFocusableInTouchMode(false);
        }
        setUrlFocusChangeInProgress(true);
        super.onUrlFocusChange(hasFocus);
    }

    @Override
    protected boolean drawChild(Canvas canvas, View child, long drawingTime) {
        boolean needsCanvasRestore = false;
        if (child == mUrlBar && mUrlActionsContainer.getVisibility() == VISIBLE) {
            canvas.save();

            // Clip the URL bar contents to ensure they do not draw under the URL actions during
            // focus animations.  Based on the RTL state of the location bar, the url actions
            // container can be on the left or right side, so clip accordingly.
            if (mUrlBar.getLeft() < mUrlActionsContainer.getLeft()) {
                canvas.clipRect(0, 0, (int) mUrlActionsContainer.getX(), getBottom());
            } else {
                canvas.clipRect(mUrlActionsContainer.getX() + mUrlActionsContainer.getWidth(),
                        0, getWidth(), getBottom());
            }
            needsCanvasRestore = true;
        }
        boolean retVal = super.drawChild(canvas, child, drawingTime);
        if (needsCanvasRestore) {
            canvas.restore();
        }
        return retVal;
    }

    /**
     * Handles any actions to be performed after all other actions triggered by the URL focus
     * change.  This will be called after any animations are performed to transition from one
     * focus state to the other.
     * @param hasFocus Whether the URL field has gained focus.
     */
    public void finishUrlFocusChange(boolean hasFocus) {
        final WindowDelegate windowDelegate = getWindowDelegate();
        if (!hasFocus) {
            // Remove the selection from the url text.  The ending selection position
            // will determine the scroll position when the url field is restored.  If
            // we do not clear this, it will scroll to the end of the text when you
            // enter/exit the tab stack.
            // We set the selection to 0 instead of removing the selection to avoid a crash that
            // happens if you clear the selection instead.
            //
            // Triggering the bug happens by:
            // 1.) Selecting some portion of the URL (where the two selection handles
            //     appear)
            // 2.) Trigger a text change in the URL bar (i.e. by triggering a new URL load
            //     by a command line intent)
            // 3.) Simultaneously moving one of the selection handles left and right.  This will
            //     occasionally throw an AssertionError on the bounds of the selection.
            if (!mUrlBar.scrollToTLD()) {
                Selection.setSelection(mUrlBar.getText(), 0);
            }

            // The animation rendering may not yet be 100% complete and hiding the keyboard makes
            // the animation quite choppy.
            postDelayed(new Runnable() {
                @Override
                public void run() {
                    UiUtils.hideKeyboard(mUrlBar);
                }
            }, KEYBOARD_HIDE_DELAY_MS);
            // Convert the keyboard back to resize mode (delay the change for an arbitrary amount
            // of time in hopes the keyboard will be completely hidden before making this change).
            if (mKeyboardResizeModeTask == null
                    && windowDelegate.getWindowSoftInputMode()
                            != WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE) {
                mKeyboardResizeModeTask = new Runnable() {
                    @Override
                    public void run() {
                        windowDelegate.setWindowSoftInputMode(
                                WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE);
                        mKeyboardResizeModeTask = null;
                    }
                };
                postDelayed(mKeyboardResizeModeTask, KEYBOARD_MODE_CHANGE_DELAY_MS);
            }
            mUrlActionsContainer.setVisibility(GONE);
        } else {
            if (mKeyboardResizeModeTask != null) {
                removeCallbacks(mKeyboardResizeModeTask);
                mKeyboardResizeModeTask = null;
            }
            if (windowDelegate.getWindowSoftInputMode()
                    != WindowManager.LayoutParams.SOFT_INPUT_ADJUST_PAN) {
                windowDelegate.setWindowSoftInputMode(
                        WindowManager.LayoutParams.SOFT_INPUT_ADJUST_PAN);
            }
            UiUtils.showKeyboard(mUrlBar);
            // As the position of the navigation icon has changed, ensure the suggestions are
            // updated to reflect the new position.
            if (getSuggestionList() != null && getSuggestionList().isShown()) {
                getSuggestionList().invalidateSuggestionViews();
            }
        }
        setUrlFocusChangeInProgress(false);

        NewTabPage ntp = getToolbarDataProvider().getNewTabPageForCurrentTab();
        if (hasFocus && ntp != null && ntp.isLocationBarShownInNTP() && mBottomSheet == null) {
            mFadingView.showFadingOverlay();
        }
    }

    @Override
    protected void updateButtonVisibility() {
        updateDeleteButtonVisibility();
        updateMicButtonVisibility(mUrlFocusChangePercent);
        updateGoogleG();
    }

    private void updateGoogleG() {
        NewTabPage ntp = getToolbarDataProvider().getNewTabPageForCurrentTab();

        // If the default search engine is not Google, isLocationBarShownInNTP() will return false.
        if (ntp == null || !ntp.isLocationBarShownInNTP()
                || !ChromeFeatureList.isEnabled(ChromeFeatureList.NTP_SHOW_GOOGLE_G_IN_OMNIBOX)) {
            mGoogleGContainer.setVisibility(View.GONE);
            return;
        }

        mGoogleGContainer.setVisibility(View.VISIBLE);
        float animationProgress =
                GOOGLE_G_FADE_INTERPOLATOR.getInterpolation(mUrlFocusChangePercent);

        final float finalGScale = 0.3f;
        // How much we have reduced the size of the G, 0 at the beginning, 0.7 at the end.
        final float shrinkingProgress = animationProgress * (1 - finalGScale);

        FrameLayout.LayoutParams layoutParams =
                (FrameLayout.LayoutParams) mGoogleG.getLayoutParams();
        layoutParams.width = Math.round(mGoogleGWidth * (1f - shrinkingProgress));

        // Shrink the margin down to 50% minus half of the G width (in the end state).
        final float finalGoogleGMargin = (mGoogleGMargin - mGoogleGWidth * finalGScale) / 2f;
        ApiCompatibilityUtils.setMarginEnd(layoutParams, Math.round(MathUtils.interpolate(
                mGoogleGMargin, finalGoogleGMargin, animationProgress)));
        // Just calling requestLayout() would not resolve the end margin.
        mGoogleG.setLayoutParams(layoutParams);

        // We want the G to be fully transparent when it is 45% of its size.
        final float scaleWhenTransparent = 0.45f;
        assert scaleWhenTransparent >= finalGScale;

        // How much we have faded out the G, 0 at the beginning, 1 when we've reduced size to 0.45.
        final float fadingProgress = Math.min(1, shrinkingProgress / (1 - scaleWhenTransparent));
        mGoogleG.setAlpha(1 - fadingProgress);
    }

    @Override
    protected void updateLocationBarIconContainerVisibility() {
        super.updateLocationBarIconContainerVisibility();
        updateIncognitoBadgePadding();
    }

    private void updateIncognitoBadgePadding() {
        // This can be triggered in the super.onFinishInflate, so we need to null check in this
        // place only.
        if (mIncognitoBadge == null) return;

        if (findViewById(R.id.location_bar_icon).getVisibility() == GONE) {
            ApiCompatibilityUtils.setPaddingRelative(
                    mIncognitoBadge, 0, 0, mIncognitoBadgePadding, 0);
        } else {
            ApiCompatibilityUtils.setPaddingRelative(mIncognitoBadge, 0, 0, 0, 0);
        }
    }

    @Override
    public void updateVisualsForState() {
        super.updateVisualsForState();

        Tab tab = getCurrentTab();
        boolean isIncognito = tab != null && tab.isIncognito();
        mIncognitoBadge.setVisibility(isIncognito ? VISIBLE : GONE);
        updateIncognitoBadgePadding();
    }

    @Override
    protected boolean shouldAnimateIconChanges() {
        return super.shouldAnimateIconChanges() || isUrlFocusChangeInProgress();
    }

    @Override
    public void setLayoutDirection(int layoutDirection) {
        super.setLayoutDirection(layoutDirection);
        updateIncognitoBadgePadding();
    }

    /**
     * Remove the update menu app menu badge.
     */
    public void removeAppMenuUpdateBadge(boolean animate) {
        boolean wasShowingMenuBadge = mShowMenuBadge;
        mShowMenuBadge = false;
        mMenuButton.setContentDescription(getResources().getString(
                R.string.accessibility_toolbar_btn_menu));

        if (!animate || !wasShowingMenuBadge) return;

        if (mIsMenuBadgeAnimationRunning && mMenuBadgeAnimatorSet != null) {
            mMenuBadgeAnimatorSet.cancel();
        }

        // Set initial states.
        mMenuButton.setAlpha(0.f);

        mMenuBadgeAnimatorSet = UpdateMenuItemHelper.createHideUpdateBadgeAnimation(
                mMenuButton, mMenuBadge);

        mMenuBadgeAnimatorSet.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationStart(Animator animation) {
                mIsMenuBadgeAnimationRunning = true;
            }

            @Override
            public void onAnimationEnd(Animator animation) {
                mIsMenuBadgeAnimationRunning = false;
            }

            @Override
            public void onAnimationCancel(Animator animation) {
                mIsMenuBadgeAnimationRunning = false;
            }
        });

        mMenuBadgeAnimatorSet.start();
    }

    public void cancelAppMenuUpdateBadgeAnimation() {
        if (mIsMenuBadgeAnimationRunning && mMenuBadgeAnimatorSet != null) {
            mMenuBadgeAnimatorSet.cancel();
        }
    }
}
