#pragma once
#include <QWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QSettings>
class PropertySection : public QWidget {
public:
    QWidget* body;
    PropertySection(const QString& title,const QString& key,QWidget* parent=nullptr):QWidget(parent) {
        auto* layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);layout->setSpacing(3);
        auto* header=new QToolButton(this);header->setObjectName("Section."+key);
        header->setText(title);header->setCheckable(true);header->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        header->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
        body=new QWidget(this);layout->addWidget(header);layout->addWidget(body);
        const auto setting="Inspector/Sections/"+key;
        const bool open=QSettings("PainEngine","Editor").value(setting,true).toBool();
        header->setChecked(open);header->setArrowType(open?Qt::DownArrow:Qt::RightArrow);body->setVisible(open);
        connect(header,&QToolButton::toggled,this,[this,header,setting](bool value){body->setVisible(value);header->setArrowType(value?Qt::DownArrow:Qt::RightArrow);QSettings("PainEngine","Editor").setValue(setting,value);});
    }
};
