#include <QtTest>
#include "core/FileClassifier.h"

using namespace bytes::core;

/// Covers: image/video/pdf/exe/zip classification, unknown extensions, no
/// extension, and the temporary-download-extension guard.
class TestClassifier : public QObject {
    Q_OBJECT
private slots:
    void imageClassification();
    void videoClassification();
    void pdfClassification();
    void exeClassification();
    void zipClassification();
    void unknownExtension();
    void noExtension();
    void temporaryDownloadExtensions();
};

void TestClassifier::imageClassification() {
    FileClassifier classifier;
    const auto result = classifier.classifyByName("photo.JPG");
    QCOMPARE(result.category, QString("Images"));
    QCOMPARE(result.extension, QString("jpg"));
}

void TestClassifier::videoClassification() {
    FileClassifier classifier;
    const auto result = classifier.classifyByName("movie.mkv");
    QCOMPARE(result.category, QString("Videos"));
}

void TestClassifier::pdfClassification() {
    FileClassifier classifier;
    const auto result = classifier.classifyByName("invoice.pdf");
    QCOMPARE(result.category, QString("Documents"));
    QCOMPARE(result.subPath, QString("PDF"));
    QCOMPARE(FileClassifier::destinationForResult(result), QString("Documents/PDF"));
}

void TestClassifier::exeClassification() {
    FileClassifier classifier;
    const auto result = classifier.classifyByName("setup.exe");
    QCOMPARE(result.category, QString("Installers"));
}

void TestClassifier::zipClassification() {
    FileClassifier classifier;
    const auto result = classifier.classifyByName("archive.zip");
    QCOMPARE(result.category, QString("Archives"));
}

void TestClassifier::unknownExtension() {
    FileClassifier classifier;
    const auto result = classifier.classifyByName("mystery.xyzabc");
    QCOMPARE(result.category, QString("Other"));
}

void TestClassifier::noExtension() {
    FileClassifier classifier;
    const auto result = classifier.classifyByName("README");
    QCOMPARE(result.category, QString("Other"));
    QVERIFY(result.extension.isEmpty());
}

void TestClassifier::temporaryDownloadExtensions() {
    QVERIFY(FileClassifier::isTemporaryDownloadName("bigfile.crdownload"));
    QVERIFY(FileClassifier::isTemporaryDownloadName("bigfile.part"));
    QVERIFY(FileClassifier::isTemporaryDownloadName("bigfile.partial"));
    QVERIFY(!FileClassifier::isTemporaryDownloadName("bigfile.mp4"));
}

QTEST_MAIN(TestClassifier)
#include "test_classifier.moc"
