#pragma once

#include <QString>

namespace locus {

/// Language preference values (stored by Prefs): 0 = System, 1 = English,
/// 2 = 简体中文.
QString locusLanguageTag(int languagePref); // "en" or "zh_CN"

/// Installs (or removes) the app translator on qApp according to the
/// preference. Widgets must be retranslated by their owners afterwards.
void installLocusTranslator(int languagePref);

} // namespace locus
