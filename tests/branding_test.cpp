#include "branding.hpp"
#include "main_window.hpp"
#include <QAction>
#include <QCryptographicHash>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QTimer>
#include <QtTest>
using namespace openece::gui;
namespace {
QByteArray read(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        qFatal("Cannot read branding asset %s", qPrintable(path));
    return file.readAll();
}
} // namespace
class BrandingTest : public QObject {
    Q_OBJECT
  private Q_SLOTS:
    void frozenArtwork() {
        const auto manifest =
            QJsonDocument::fromJson(read(QString(OPENECE_SOURCE_DIR) +
                                         "/assets/branding/IMPORTED-ASSET-SHA256SUMS.json"))
                .object();
        QVERIFY(!manifest.isEmpty());
        for (auto it = manifest.begin(); it != manifest.end(); ++it)
            QCOMPARE(QString::fromLatin1(QCryptographicHash::hash(
                                             read(QString(OPENECE_SOURCE_DIR) + "/" + it.key()),
                                             QCryptographicHash::Sha256)
                                             .toHex()),
                     it.value().toString());
    }
    void staticResourcesAndMetadata() {
        configure_application_branding();
        QCOMPARE(QApplication::applicationName(), QString("OpenECE"));
        QCOMPARE(QApplication::organizationName(), QString("OpenECE"));
        QCOMPARE(QApplication::applicationVersion(), QString(OPENECE_TEST_VERSION));
        const auto icon = application_icon();
        QVERIFY(!QApplication::windowIcon().isNull());
        for (const int size : {16, 20, 24, 32, 40, 48, 64, 96, 128, 256, 512, 1024}) {
            const auto embedded = read(QString(":/branding/openece-%1.png").arg(size));
            QCOMPARE(embedded, read(QString(OPENECE_SOURCE_DIR) +
                                    QString("/assets/branding/png/openece-%1.png").arg(size)));
            QCOMPARE(QImage::fromData(embedded).size(), QSize(size, size));
            QVERIFY(icon.availableSizes().contains(QSize(size, size)));
            QVERIFY(!icon.pixmap(size, size).isNull());
        }
        MainWindow window;
        QVERIFY(!window.windowIcon().isNull());
    }
    void brandedAbout() {
        MainWindow window;
        bool inspected = false;
        QTimer::singleShot(0, &window, [&] {
            if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
                inspected = box->objectName() == "about_openece_dialog" &&
                            box->textFormat() == Qt::PlainText && !box->iconPixmap().isNull() &&
                            box->text().contains(OPENECE_TEST_VERSION) &&
                            box->text().contains("MIT") &&
                            box->text().contains("THIRD-PARTY-NOTICES") &&
                            box->text().contains("github.com/codyklein/open-ece") &&
                            box->text().contains("results are recomputed after Open");
                box->accept();
            }
        });
        window.findChild<QAction*>("about_openece")->trigger();
        QVERIFY(inspected);
    }
};
QTEST_MAIN(BrandingTest)
#include "branding_test.moc"
