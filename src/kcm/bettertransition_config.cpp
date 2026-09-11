/*
    SPDX-FileCopyrightText: 2026 The Better Transition effect authors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "bettertransition_config.h"

// KConfigSkeleton
#include "bettertransitionconfig.h"

#include "kwineffects_interface.h"

#include <KPluginFactory>

#include <QDBusConnection>

namespace KWin
{

K_PLUGIN_CLASS(BetterTransitionEffectConfig)

BetterTransitionEffectConfig::BetterTransitionEffectConfig(QObject *parent, const KPluginMetaData &data)
    : KCModule(parent, data)
{
    ui.setupUi(widget());
    BetterTransitionConfig::instance(QStringLiteral("kwinrc"));
    addConfig(BetterTransitionConfig::self(), widget());
}

void BetterTransitionEffectConfig::save()
{
    KCModule::save();

    OrgKdeKwinEffectsInterface interface(QStringLiteral("org.kde.KWin"),
                                         QStringLiteral("/Effects"),
                                         QDBusConnection::sessionBus());
    interface.reconfigureEffect(QStringLiteral("bettertransition"));
}

} // namespace KWin

#include "bettertransition_config.moc"

#include "moc_bettertransition_config.cpp"
