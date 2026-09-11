/*
    SPDX-FileCopyrightText: 2026 The Better Transition effect authors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "bettertransition.h"

// KConfigSkeleton
#include "bettertransitionconfig.h"

#include "effect/effecthandler.h"

#include <core/output.h>

#include <QRegion>

#include <algorithm>
#include <utility>

namespace KWin
{

static inline QRectF toQRectF(const RectF &rect)
{
    return QRectF(rect.x(), rect.y(), rect.width(), rect.height());
}

static inline qreal geometryArea(const RectF &rect)
{
    return rect.width() * rect.height();
}

BetterTransitionEffect::BetterTransitionEffect()
{
    BetterTransitionConfig::instance(effects->config());
    reconfigure(ReconfigureAll);

    connect(effects, &EffectsHandler::stackingOrderChanged,
            this, &BetterTransitionEffect::slotStackingOrderChanged);
    connect(effects, &EffectsHandler::windowAdded,
            this, &BetterTransitionEffect::slotWindowAdded);
    connect(effects, &EffectsHandler::windowDeleted,
            this, &BetterTransitionEffect::slotWindowDeleted);

    const auto windows = effects->stackingOrder();
    for (EffectWindow *window : windows) {
        slotWindowAdded(window);
    }

    m_previousOrder = effects->stackingOrder();
}

BetterTransitionEffect::~BetterTransitionEffect()
{
    // Make sure we never leave a window stuck on top of the stack.
    for (auto it = m_animations.constBegin(); it != m_animations.constEnd(); ++it) {
        if (it->elevated && it.key() && it.key()->windowItem()) {
            effects->setElevatedWindow(it.key(), false);
        }
    }
    m_animations.clear();
    m_selfFades.clear();
    m_scaleIns.clear();
    m_elevationRefs.clear();
    m_minimizedWindows.clear();
}

bool BetterTransitionEffect::supported()
{
    return effects->animationsSupported();
}

void BetterTransitionEffect::reconfigure(ReconfigureFlags flags)
{
    Q_UNUSED(flags)

    BetterTransitionConfig::self()->read();

    m_fadeOutDuration = animationTime(std::chrono::milliseconds(BetterTransitionConfig::fadeOutDuration()));
    m_holdDuration = animationTime(std::chrono::milliseconds(BetterTransitionConfig::holdDuration()));
    m_fadeInDuration = animationTime(std::chrono::milliseconds(BetterTransitionConfig::fadeInDuration()));
    m_targetOpacity = 1.0 - std::clamp(BetterTransitionConfig::fadeStrength() / 100.0, 0.0, 1.0);
    m_onlyOverlapping = BetterTransitionConfig::onlyOverlapping();
    m_includeSpecialWindows = BetterTransitionConfig::includeSpecialWindows();

    m_largeCovererScreenRatio = std::clamp(BetterTransitionConfig::largeCovererScreenRatio() / 100.0, 0.0, 1.0);

    m_scaleInDuration = animationTime(std::chrono::milliseconds(BetterTransitionConfig::scaleInDuration()));
    m_scaleInStartScale = std::clamp(BetterTransitionConfig::scaleInStartScale() / 100.0, 0.1, 1.0);
    m_scaleInThreshold = std::clamp(BetterTransitionConfig::scaleInThreshold() / 100.0, 0.0, 1.0);

    effects->addRepaintFull();
}

void BetterTransitionEffect::slotWindowAdded(EffectWindow *w)
{
    if (w->isMinimized()) {
        m_minimizedWindows.insert(w);
    }

    connect(w, &EffectWindow::minimizedChanged, this, [this](EffectWindow *window) {
        if (window->isMinimized()) {
            m_minimizedWindows.insert(window);
        }
        // Deliberately keep the entry when the window is unminimized: it is
        // consumed below, when the window is raised.
    });
}

void BetterTransitionEffect::pruneMinimizedWindows()
{
    // A window that has been unminimized but did not cause this raise (for
    // example a transient restored together with its main window) must not
    // swallow a later, genuine raise, so drop it from the set.
    for (auto it = m_minimizedWindows.begin(); it != m_minimizedWindows.end();) {
        EffectWindow *w = *it;
        if (!w->isMinimized()) {
            it = m_minimizedWindows.erase(it);
        } else {
            ++it;
        }
    }
}

void BetterTransitionEffect::slotStackingOrderChanged()
{
    const QList<EffectWindow *> newOrder = effects->stackingOrder();

    if (effects->activeFullScreenEffect() || newOrder == m_previousOrder) {
        pruneMinimizedWindows();
        m_previousOrder = newOrder;
        return;
    }

    // The raised window is the topmost window whose position in the stacking
    // order has increased. Windows that were just mapped are ignored - they are
    // not in the previous order at all.
    EffectWindow *raised = nullptr;
    int raisedIndex = -1;
    for (int i = 0; i < newOrder.size(); ++i) {
        EffectWindow *candidate = newOrder.at(i);
        const int oldIndex = m_previousOrder.indexOf(candidate);
        if (oldIndex < 0) {
            continue;
        }
        if (i > oldIndex && i > raisedIndex && isUsableWindow(candidate)) {
            raised = candidate;
            raisedIndex = i;
        }
    }

    if (!raised || !raised->isOnCurrentDesktop() || !raised->isOnCurrentActivity()) {
        pruneMinimizedWindows();
        m_previousOrder = newOrder;
        return;
    }

    if (m_minimizedWindows.remove(raised)) {
        // The window was minimized before, so this raise is only the restore
        // from minimization. Neither the fade nor the scale-in should run.
        pruneMinimizedWindows();
        m_previousOrder = newOrder;
        return;
    }

    {
        // Every window that was covering the raised window before the raise.
        QList<EffectWindow *> coverers;
        const int oldRaisedIndex = m_previousOrder.indexOf(raised);
        for (int i = oldRaisedIndex + 1; i < m_previousOrder.size(); ++i) {
            EffectWindow *candidate = m_previousOrder.at(i);
            if (candidate == raised || !isUsableWindow(candidate)) {
                continue;
            }
            if (!candidate->isOnCurrentDesktop() || !candidate->isOnCurrentActivity()) {
                continue;
            }
            if (m_onlyOverlapping && !occludes(candidate, raised)) {
                continue;
            }
            coverers.append(candidate);
        }

        if (!coverers.isEmpty()) {
            // Largest covering window and the area of the screen it is on, for
            // the "huge coverer" rule.
            EffectWindow *largestCoverer = nullptr;
            qreal largestCovererArea = 0.0;
            for (EffectWindow *coverer : std::as_const(coverers)) {
                const qreal area = geometryArea(revealGeometry(coverer));
                if (area > largestCovererArea) {
                    largestCovererArea = area;
                    largestCoverer = coverer;
                }
            }
            const qreal screenArea = screenAreaFor(largestCoverer);
            const qreal raisedArea = geometryArea(revealGeometry(raised));

            if (occlusionRatio(raised, coverers) > m_scaleInThreshold) {
                // The raised window was almost completely hidden. Fading the
                // windows in front of it would barely read as a reveal, so
                // scale the raised window itself in instead.
                startScaleIn(raised);
            } else if (largestCovererArea > raisedArea
                       && screenArea > 0.0
                       && largestCovererArea >= m_largeCovererScreenRatio * screenArea) {
                // A single huge window covers the raised window. Fading that
                // window out would be jarring, so the raised window performs the
                // fade-out / hold / fade-in itself instead.
                startSelfFade(raised);
            } else {
                // Only coverers that end up behind the raised window can fade
                // back in behind it.
                for (EffectWindow *occluder : std::as_const(coverers)) {
                    const int newIndex = newOrder.indexOf(occluder);
                    if (newIndex < 0 || newIndex >= raisedIndex) {
                        continue;
                    }
                    qreal startFactor = 1.0;
                    const auto animationIt = m_animations.constFind(occluder);
                    if (animationIt != m_animations.constEnd()) {
                        startFactor = factorFor(*animationIt);
                    }
                    startAnimation(occluder, raised, startFactor);
                }
            }
        }
    }

    pruneMinimizedWindows();
    m_previousOrder = newOrder;
}

void BetterTransitionEffect::startAnimation(EffectWindow *occluder, EffectWindow *raised, qreal startFactor)
{
    const std::chrono::milliseconds total = m_fadeOutDuration + m_holdDuration + m_fadeInDuration;
    if (total <= std::chrono::milliseconds::zero() || !occluder || !occluder->windowItem()) {
        return;
    }

    auto animationIt = m_animations.find(occluder);
    if (animationIt != m_animations.end()) {
        // Restart an interrupted animation smoothly from its current opacity.
        Animation &animation = *animationIt;
        animation.raised = raised;
        animation.startFactor = std::clamp(startFactor, 0.0, 1.0);
        animation.targetFactor = m_targetOpacity;
        animation.timeLine.setDuration(total);
        animation.timeLine.setDirection(TimeLine::Forward);
        animation.timeLine.setEasingCurve(QEasingCurve::Linear);
        animation.timeLine.reset();
        if (!animation.elevated) {
            acquireElevation(occluder);
            animation.elevated = true;
        }
        occluder->addRepaintFull();
        return;
    }

    Animation animation;
    animation.raised = raised;
    animation.startFactor = std::clamp(startFactor, 0.0, 1.0);
    animation.targetFactor = m_targetOpacity;
    animation.timeLine = TimeLine(total, TimeLine::Forward);
    animation.timeLine.setEasingCurve(QEasingCurve::Linear);

    acquireElevation(occluder);
    animation.elevated = true;

    m_animations.insert(occluder, animation);
    occluder->addRepaintFull();
}

void BetterTransitionEffect::startSelfFade(EffectWindow *raised)
{
    // The raise is already committed by the time this runs, so playing a
    // fade-out would first show the window popping to the front. Start right
    // away at the minimum opacity and only play the hold + fade-in.
    const std::chrono::milliseconds total = m_holdDuration + m_fadeInDuration;
    if (total <= std::chrono::milliseconds::zero() || !raised || !raised->windowItem()) {
        return;
    }

    auto animationIt = m_selfFades.find(raised);
    if (animationIt != m_selfFades.end()) {
        Animation &animation = *animationIt;
        animation.targetFactor = m_targetOpacity;
        animation.timeLine.setDuration(total);
        animation.timeLine.setDirection(TimeLine::Forward);
        animation.timeLine.setEasingCurve(QEasingCurve::Linear);
        animation.timeLine.reset();
    } else {
        Animation animation;
        animation.targetFactor = m_targetOpacity;
        animation.timeLine = TimeLine(total, TimeLine::Forward);
        animation.timeLine.setEasingCurve(QEasingCurve::Linear);
        m_selfFades.insert(raised, animation);
    }

    raised->addRepaintFull();
    effects->addRepaintFull();
}

void BetterTransitionEffect::startScaleIn(EffectWindow *raised)
{
    if (!raised || !raised->windowItem()) {
        return;
    }

    auto scaleInIt = m_scaleIns.find(raised);
    if (scaleInIt != m_scaleIns.end()) {
        scaleInIt->timeLine.setDuration(m_scaleInDuration);
        scaleInIt->timeLine.reset();
    } else {
        ScaleInAnimation animation;
        animation.timeLine = TimeLine(m_scaleInDuration, TimeLine::Forward);
        animation.timeLine.setEasingCurve(QEasingCurve::Linear);
        m_scaleIns.insert(raised, animation);
    }

    raised->addRepaintFull();
    effects->addRepaintFull();
}

qreal BetterTransitionEffect::factorFor(const Animation &animation) const
{
    const qreal total = animation.timeLine.duration().count();
    if (total <= 0.0) {
        return 1.0;
    }

    const qreal progress = std::clamp(animation.timeLine.progress(), 0.0, 1.0);
    const qreal fadeOutFraction = m_fadeOutDuration.count() / total;
    const qreal holdFraction = m_holdDuration.count() / total;
    const qreal fadeInFraction = m_fadeInDuration.count() / total;

    if (fadeOutFraction > 0.0 && progress < fadeOutFraction) {
        const qreal t = QEasingCurve(QEasingCurve::OutCubic).valueForProgress(progress / fadeOutFraction);
        return interpolate(animation.startFactor, animation.targetFactor, t);
    }

    if (progress < fadeOutFraction + holdFraction) {
        return animation.targetFactor;
    }

    if (fadeInFraction <= 0.0) {
        return 1.0;
    }

    const qreal t = QEasingCurve(QEasingCurve::InOutSine).valueForProgress(
        (progress - fadeOutFraction - holdFraction) / fadeInFraction);
    return interpolate(animation.targetFactor, 1.0, t);
}

qreal BetterTransitionEffect::selfFadeFactorFor(const Animation &animation) const
{
    const qreal total = animation.timeLine.duration().count();
    if (total <= 0.0) {
        return 1.0;
    }

    const qreal progress = std::clamp(animation.timeLine.progress(), 0.0, 1.0);
    const qreal holdFraction = m_holdDuration.count() / total;

    if (progress < holdFraction) {
        return animation.targetFactor;
    }

    const qreal fadeInFraction = m_fadeInDuration.count() / total;
    if (fadeInFraction <= 0.0) {
        return 1.0;
    }

    const qreal t = QEasingCurve(QEasingCurve::InOutSine).valueForProgress(
        (progress - holdFraction) / fadeInFraction);
    return interpolate(animation.targetFactor, 1.0, t);
}

qreal BetterTransitionEffect::occlusionRatio(const EffectWindow *raised, const QList<EffectWindow *> &coverers) const
{
    const RectF raisedGeometry = revealGeometry(raised);
    const qreal raisedArea = raisedGeometry.width() * raisedGeometry.height();
    if (raisedArea <= 0.0) {
        return 0.0;
    }

    const QRectF raisedRect = toQRectF(raisedGeometry);

    // Accumulate the covered area in a region so that overlapping coverers are
    // not counted twice.
    QRegion occluded;
    for (const EffectWindow *coverer : coverers) {
        const QRectF covererRect = toQRectF(revealGeometry(coverer));
        const QRectF intersection = raisedRect.intersected(covererRect);
        if (!intersection.isEmpty()) {
            occluded += intersection.toAlignedRect();
        }
    }

    qreal occludedArea = 0.0;
    for (const QRect &rect : occluded) {
        occludedArea += qreal(rect.width()) * qreal(rect.height());
    }

    return std::clamp(occludedArea / raisedArea, 0.0, 1.0);
}

qreal BetterTransitionEffect::screenAreaFor(const EffectWindow *w) const
{
    if (w) {
        if (const LogicalOutput *output = w->screen()) {
            const Rect geometry = output->geometry();
            const qreal area = qreal(geometry.width()) * qreal(geometry.height());
            if (area > 0.0) {
                return area;
            }
        }
    }

    const Rect geometry = effects->virtualScreenGeometry();
    return qreal(geometry.width()) * qreal(geometry.height());
}

void BetterTransitionEffect::prePaintScreen(ScreenPrePaintData &data)
{
    for (auto it = m_animations.begin(); it != m_animations.end(); ++it) {
        it->timeLine.advance(data.view);
    }
    for (auto it = m_selfFades.begin(); it != m_selfFades.end(); ++it) {
        it->timeLine.advance(data.view);
    }
    for (auto it = m_scaleIns.begin(); it != m_scaleIns.end(); ++it) {
        it->timeLine.advance(data.view);
    }

    effects->prePaintScreen(data);
}

void BetterTransitionEffect::prePaintWindow(RenderView *view, EffectWindow *w, WindowPrePaintData &data)
{
    if (m_animations.contains(w) || m_selfFades.contains(w)) {
        // The window is painted with a reduced opacity, so the scene has to
        // repaint what is behind it as well.
        data.setTranslucent();
    }
    if (m_scaleIns.contains(w)) {
        // The window is scaled, so the area it covers changes every frame.
        data.setTransformed();
    }

    effects->prePaintWindow(view, w, data);
}

void BetterTransitionEffect::paintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const Region &deviceRegion, WindowPaintData &data)
{
    const auto animationIt = m_animations.constFind(w);
    if (animationIt != m_animations.constEnd()) {
        data.multiplyOpacity(factorFor(*animationIt));
    }

    const auto selfFadeIt = m_selfFades.constFind(w);
    if (selfFadeIt != m_selfFades.constEnd()) {
        data.multiplyOpacity(selfFadeFactorFor(*selfFadeIt));
    }

    const auto scaleInIt = m_scaleIns.constFind(w);
    if (scaleInIt != m_scaleIns.constEnd()) {
        const qreal progress = std::clamp(scaleInIt->timeLine.progress(), 0.0, 1.0);
        // OutCubic is monotonic: the window only grows, without any bounce or
        // overshoot.
        const qreal eased = QEasingCurve(QEasingCurve::OutCubic).valueForProgress(progress);
        const qreal scale = interpolate(m_scaleInStartScale, 1.0, eased);

        // Scale around the center of the window, matching what
        // AnimationEffect does for a centered anchor.
        const QSizeF size = w->frameGeometry().size();
        data.translate(0.5 * (1.0 - scale) * size.width(),
                       0.5 * (1.0 - scale) * size.height());
        data.setXScale(data.xScale() * scale);
        data.setYScale(data.yScale() * scale);
    }

    effects->paintWindow(renderTarget, viewport, w, mask, deviceRegion, data);
}

void BetterTransitionEffect::postPaintScreen()
{
    const std::chrono::milliseconds releaseThreshold = m_fadeOutDuration + m_holdDuration;

    for (auto it = m_animations.begin(); it != m_animations.end();) {
        EffectWindow *w = it.key();
        Animation &animation = *it;

        w->addRepaintFull();
        if (animation.raised) {
            animation.raised->addRepaintFull();
        }

        // Once the hold phase is over the occluder must drop behind the raised
        // window, where it fades back in.
        if (animation.elevated && animation.timeLine.elapsed() >= releaseThreshold) {
            releaseElevation(w);
            animation.elevated = false;
        }

        if (animation.timeLine.done()) {
            if (animation.elevated) {
                releaseElevation(w);
                animation.elevated = false;
            }
            it = m_animations.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it = m_scaleIns.begin(); it != m_scaleIns.end();) {
        EffectWindow *w = it.key();
        w->addRepaintFull();
        if (it->timeLine.done()) {
            it = m_scaleIns.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it = m_selfFades.begin(); it != m_selfFades.end();) {
        EffectWindow *w = it.key();
        w->addRepaintFull();
        if (it->timeLine.done()) {
            it = m_selfFades.erase(it);
        } else {
            ++it;
        }
    }

    effects->postPaintScreen();
}

bool BetterTransitionEffect::isActive() const
{
    return !m_animations.isEmpty() || !m_selfFades.isEmpty() || !m_scaleIns.isEmpty();
}

bool BetterTransitionEffect::blocksDirectScanout() const
{
    return !m_animations.isEmpty() || !m_selfFades.isEmpty() || !m_scaleIns.isEmpty();
}

int BetterTransitionEffect::requestedEffectChainPosition() const
{
    return 50;
}

void BetterTransitionEffect::slotWindowDeleted(EffectWindow *w)
{
    m_animations.remove(w);
    m_selfFades.remove(w);
    m_scaleIns.remove(w);
    m_elevationRefs.remove(w);
    m_minimizedWindows.remove(w);
    m_previousOrder.removeAll(w);

    for (auto it = m_animations.begin(); it != m_animations.end(); ++it) {
        if (it->raised == w) {
            it->raised = nullptr;
        }
    }
}

bool BetterTransitionEffect::isUsableWindow(const EffectWindow *w) const
{
    if (!w || w->isDeleted() || w->isMinimized() || !w->isVisible()) {
        return false;
    }

    if (w->isNormalWindow() || w->isDialog() || w->isUtility()) {
        return true;
    }

    if (!m_includeSpecialWindows) {
        return false;
    }

    // Never touch these, they are not part of the normal window stack.
    if (w->isDesktop() || w->isDock() || w->isPopupWindow() || w->isInputMethod()
        || w->isLockScreen() || w->isOutline() || w->isDNDIcon()) {
        return false;
    }

    if (w->isX11Client() && !w->isManaged()) {
        return false;
    }

    return true;
}

RectF BetterTransitionEffect::revealGeometry(const EffectWindow *w) const
{
    RectF geometry = w->frameGeometry();
    if (w->isModal()) {
        const auto mainWindows = w->mainWindows();
        for (const EffectWindow *mainWindow : mainWindows) {
            geometry = geometry.united(mainWindow->frameGeometry());
        }
    }
    return geometry;
}

bool BetterTransitionEffect::occludes(const EffectWindow *occluder, const EffectWindow *raised) const
{
    if (!m_onlyOverlapping) {
        return true;
    }
    return revealGeometry(occluder).intersects(revealGeometry(raised));
}

void BetterTransitionEffect::acquireElevation(EffectWindow *w)
{
    if (!w || !w->windowItem()) {
        return;
    }

    const int refs = m_elevationRefs.value(w, 0);
    m_elevationRefs.insert(w, refs + 1);
    if (refs == 0) {
        effects->setElevatedWindow(w, true);
    }
}

void BetterTransitionEffect::releaseElevation(EffectWindow *w)
{
    if (!w) {
        return;
    }

    auto it = m_elevationRefs.find(w);
    if (it == m_elevationRefs.end()) {
        return;
    }

    if (*it <= 1) {
        m_elevationRefs.erase(it);
        if (w->windowItem()) {
            effects->setElevatedWindow(w, false);
        }
    } else {
        --(*it);
    }
}

} // namespace KWin

#include "moc_bettertransition.cpp"
