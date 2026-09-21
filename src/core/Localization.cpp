#include "core/Localization.h"

#include <QCoreApplication>
#include <QLocale>
#include <QString>
#include <QTranslator>

namespace locus {

QString locusLanguageTag(int languagePref) {
  if (languagePref == 2)
    return QStringLiteral("zh_CN");
  if (languagePref == 1)
    return QStringLiteral("en");
  // System: any Chinese UI language maps to zh_CN, everything else to en.
  const QStringList uiLanguages = QLocale().uiLanguages();
  for (const QString &lang : uiLanguages) {
    if (lang.startsWith(QLatin1String("zh"), Qt::CaseInsensitive))
      return QStringLiteral("zh_CN");
  }
  return QStringLiteral("en");
}

void installLocusTranslator(int languagePref) {
  static QTranslator *translator = nullptr;
  if (translator) {
    QCoreApplication::removeTranslator(translator);
    delete translator;
    translator = nullptr;
  }
  if (locusLanguageTag(languagePref) != QStringLiteral("zh_CN"))
    return;
  auto *t = new QTranslator(QCoreApplication::instance());
  // .qm files are embedded via qt_add_translations (RESOURCE_PREFIX "/i18n").
  if (t->load(QStringLiteral("locus_zh_CN"), QStringLiteral(":/i18n"))) {
    translator = t;
    QCoreApplication::installTranslator(translator);
  } else {
    delete t;
  }
}

} // namespace locus
