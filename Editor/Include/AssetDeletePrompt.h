#pragma once

#include <QMessageBox>
#include <QPushButton>
#include <QString>
#include <QWidget>

namespace EditorUI
{

inline QMessageBox* CreateDeleteConfirmationBox(QWidget* parent, QMessageBox::Icon icon, const QString& title, const QString& text)
{
    auto* box = new QMessageBox(icon, title, text, QMessageBox::Yes | QMessageBox::No, parent);
    box->setDefaultButton(QMessageBox::Yes);
    box->setEscapeButton(QMessageBox::No);
    return box;
}

inline bool ExecDeleteConfirmation(QWidget* parent, QMessageBox::Icon icon, const QString& title, const QString& text)
{
    auto* box = CreateDeleteConfirmationBox(parent, icon, title, text);
    const int result = box->exec();
    box->deleteLater();
    return result == QMessageBox::Yes;
}

} // namespace EditorUI

