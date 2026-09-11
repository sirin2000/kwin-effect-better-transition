/*
    SPDX-FileCopyrightText: 2026 The Better Transition effect authors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "bettertransition.h"

namespace KWin
{

KWIN_EFFECT_FACTORY_SUPPORTED(BetterTransitionEffect,
                              "metadata.json",
                              return BetterTransitionEffect::supported();)

} // namespace KWin

#include "main.moc"
