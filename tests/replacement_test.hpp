#pragma once
#include <QApplication>
#include <QMessageBox>
#include <QTimer>
#include <QtTest>
inline void accept_replacement(QWidget& owner) {
    QTimer::singleShot(0, &owner, [] {
        auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        QVERIFY(box);
        QCOMPARE(box->defaultButton(), box->button(QMessageBox::Cancel));
        QCOMPARE(box->textFormat(), Qt::PlainText);
        box->done(QMessageBox::Yes);
    });
}
