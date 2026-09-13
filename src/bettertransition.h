/*
    SPDX-FileCopyrightText: 2026 The Better Transition effect authors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <core/rect.h>
#include <effect/effect.h>
#include <effect/effectwindow.h>
#include <effect/timeline.h>

#include <QHash>
#include <QList>
#include <QSet>
#include <QSizeF>

#include <chrono>

namespace KWin
{

/**
 * Better Transition
 *
 * When a window is raised above the windows that were covering it, the covering
 * windows fade out first, stay hidden for a short moment, and then fade back in
 * - but now behind the raised window, in their original relative stacking order.
 * Only windows that actually end up behind the raised window count as coverers;
 * a window that stays above it (e.g. a keep-above surface) is ignored, because
 * it cannot be revealed that way.
 *
 * Two special cases take precedence over fading the covering windows:
 *
 * 1. If the raised window was almost completely covered (more than
 *    ScaleInThreshold percent of its area), it is scaled in from
 *    ScaleInStartScale to its normal size.
 *
 * 2. Otherwise, if a single covering window takes up at least
 *    LargeCovererScreenRatio percent of its screen and is larger than the
 *    raised window, fading that huge window out would be jarring, so the raised
 *    window itself fades in instead. The raise is already committed by the time
 *    the effect learns about it, so the raised window starts out at the minimum
 *    opacity (i.e. invisible) on the very frame it is raised - there is no
 *    visible pop to the front - and only the hold + fade-in are played.
 *
 * A raise that is only the restore of a previously minimized window is not
 * animated at all.
 *
 * The effect never reorders windows itself. For the fade path it only
 * (a) temporarily paints the covering windows on top of the stack while they
 * fade out, so the reveal is actually visible, and (b) multiplies their opacity
 * while the animation runs.
 */
class BetterTransitionEffect : public Effect
{
    Q_OBJECT

public:
    BetterTransitionEffect();
    ~BetterTransitionEffect() override;

    static bool supported();

    void reconfigure(ReconfigureFlags flags) override;

    void prePaintScreen(ScreenPrePaintData &data) override;
    void prePaintWindow(RenderView *view, EffectWindow *w, WindowPrePaintData &data) override;
    void paintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const Region &deviceRegion, WindowPaintData &data) override;
    void postPaintScreen() override;

    bool isActive() const override;
    bool blocksDirectScanout() const override;
    int requestedEffectChainPosition() const override;

private Q_SLOTS:
    void slotWindowAdded(EffectWindow *w);
    void slotWindowClosed(EffectWindow *w);
    void slotStackingOrderChanged();
    void slotWindowDeleted(EffectWindow *w);

private:
    struct Animation
    {
        // Window that is being revealed. May become null if it is closed.
        EffectWindow *raised = nullptr;
        // Linear timeline spanning fade-out + hold + fade-in.
        TimeLine timeLine;
        // Opacity the fade-out starts from (mid-flight restarts are smooth).
        qreal startFactor = 1.0;
        // Opacity held during the hold phase.
        qreal targetFactor = 0.0;
        // Whether this effect currently holds an elevation for the window.
        bool elevated = false;
        // Phase durations snapshotted when the animation was (re)started, so a
        // reconfigure() while it is running cannot skew the phase mapping.
        std::chrono::milliseconds fadeOutDuration{0};
        std::chrono::milliseconds holdDuration{0};
        std::chrono::milliseconds fadeInDuration{0};
    };

    // Fades the raised window itself: hold + fade-in only.
    struct SelfFadeAnimation
    {
        TimeLine timeLine;
        qreal targetFactor = 0.0;
        std::chrono::milliseconds holdDuration{0};
        std::chrono::milliseconds fadeInDuration{0};
    };

    struct ScaleInAnimation
    {
        // Linear timeline; the easing curve is applied when sampling it.
        TimeLine timeLine;
        // Start scale snapshotted at animation start.
        qreal startScale = 1.0;
    };

    // Fades the covering windows out and back in behind the raised window.
    void startAnimation(EffectWindow *occluder, EffectWindow *raised, qreal startFactor);
    // Fades the raised window itself in (huge coverer case).
    void startSelfFade(EffectWindow *raised);
    void startScaleIn(EffectWindow *raised);
    qreal factorFor(const Animation &animation) const;
    qreal selfFadeFactorFor(const SelfFadeAnimation &animation) const;
    qreal occlusionRatio(const EffectWindow *raised, const QList<EffectWindow *> &coverers) const;
    qreal screenAreaFor(const EffectWindow *w) const;

    bool isUsableWindow(const EffectWindow *w) const;
    RectF revealGeometry(const EffectWindow *w) const;
    bool occludes(const EffectWindow *occluder, const EffectWindow *raised) const;

    void acquireElevation(EffectWindow *w);
    void releaseElevation(EffectWindow *w);
    void pruneMinimizedWindows();

    QHash<EffectWindow *, Animation> m_animations;
    QHash<EffectWindow *, SelfFadeAnimation> m_selfFades;
    QHash<EffectWindow *, ScaleInAnimation> m_scaleIns;
    // Opacity frozen at the moment a window was closed, so our fade does not
    // fight with the close animation (mirrors what the Dim Inactive effect
    // does). Cleared in slotWindowDeleted().
    QHash<EffectWindow *, qreal> m_frozenFade;
    QHash<EffectWindow *, qreal> m_frozenSelfFade;
    QHash<EffectWindow *, int> m_elevationRefs;
    QList<EffectWindow *> m_previousOrder;
    // Windows that are minimized (or were minimized and not restored yet).
    // A raise of such a window is a restore-from-minimize, which is not animated.
    QSet<EffectWindow *> m_minimizedWindows;

    std::chrono::milliseconds m_fadeOutDuration{150};
    std::chrono::milliseconds m_holdDuration{700};
    std::chrono::milliseconds m_fadeInDuration{250};
    qreal m_targetOpacity = 0.0;
    bool m_onlyOverlapping = true;
    bool m_includeSpecialWindows = false;
    // Paint the covering windows above the raised window while they fade out.
    // Off by default: their inactive decorations/shadows would briefly cover
    // the raised window.
    bool m_elevateCoveringWindows = false;

    qreal m_largeCovererScreenRatio = 0.75;

    std::chrono::milliseconds m_scaleInDuration{400};
    qreal m_scaleInStartScale = 0.9;
    qreal m_scaleInThreshold = 0.9;
};

} // namespace KWin
