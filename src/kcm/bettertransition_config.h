/*
    SPDX-FileCopyrightText: 2026 The Better Transition effect authors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_bettertransition_config.h"

#include <KCModule>
#include <KPluginMetaData>

namespace KWin
{

class BetterTransitionEffectConfig : public KCModule
{
    Q_OBJECT

public:
    explicit BetterTransitionEffectConfig(QObject *parent, const KPluginMetaData &data);

    void save() override;

private:
    ::Ui::BetterTransitionEffectConfig ui;
};

} // namespace KWin
