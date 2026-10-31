#include "ObjectInspectorPanel.h"
#include "PropertySection.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QFormLayout>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QSignalBlocker>
#include <QApplication>
#include <QFileDialog>
#include <QPushButton>
#include <QDir>
#include <QMap>
#include <cmath>
namespace {
QJsonValue Read(const QJsonObject& data,const QString& path) {
    QJsonValue value(data);
    for(auto token:path.mid(1).split('/')) {
        token.replace("~1","/");token.replace("~0","~");
        if(value.isArray()){bool ok=false;int index=token.toInt(&ok);value=ok?value.toArray().at(index):QJsonValue{};}
        else value=value.toObject().value(token);
    }
    return value;
}
bool Editing(QWidget* widget){auto* focus=QApplication::focusWidget();return focus&&(focus==widget||widget->isAncestorOf(focus));}
}
void ObjectInspectorPanel::SubmitProperty(const QString& path,const QJsonValue& value) {
    if(refreshing_||!selectedId_)return;
    auto bytes=QJsonDocument(QJsonArray{value}).toJson(QJsonDocument::Compact);
    emit PropertyEditingFinished(selectedId_,path,QString::fromUtf8(bytes.mid(1,bytes.size()-2)));
}
void ObjectInspectorPanel::RefreshProjectProperties(const EditorQtHost::Snapshot& view,const EditorQtHost::SceneObject& object) {
    const auto schema=QJsonDocument::fromJson(QByteArray::fromStdString(object.propertySchema)).object();
    const auto fields=schema["fields"].toArray();
    properties_->setVisible(!fields.isEmpty());
    QString key=QString::number(object.id)+QString::fromStdString(object.propertySchema);
    // Object reference selectors track names/IDs, while numeric edits do not
    // rebuild controls on each scene refresh or steal text-entry focus.
    bool references=false;for(const auto& value:fields)if(value.toObject()["type"]=="object")references=true;
    if(references)for(const auto& row:view.objects)key+=QString::number(row.id)+QString::fromStdString(row.name);
    if(key!=propertiesKey_) {
        propertiesKey_=key;propertyRefresh_.clear();
        while(auto* item=propertiesLayout_->takeAt(0)){delete item->widget();delete item;}
        QMap<QString,PropertySection*> sections;QMap<QString,QFormLayout*> forms;
        for(const auto& value:fields) {
            const auto field=value.toObject();const auto path=field["path"].toString(),type=field["type"].toString();
            if(!path.startsWith('/')||path=="/version")continue;
            auto group=field["group"].toString("Свойства класса");if(group.isEmpty())group="Свойства класса";
            auto* layout=propertiesLayout_;QString prefix;
            for(const auto& part:group.split('/',Qt::SkipEmptyParts)) {
                prefix+=(prefix.isEmpty()?QString():QString("/"))+part;
                if(!sections.contains(prefix)) {
                    auto* section=new PropertySection(part,QString::fromStdString(object.type)+"/"+prefix,properties_);
                    sections[prefix]=section;layout->addWidget(section);
                    auto* nested=new QVBoxLayout(section->body);auto* form=new QFormLayout;form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);nested->addLayout(form);forms[prefix]=form;
                }
                layout=qobject_cast<QVBoxLayout*>(sections[prefix]->body->layout());
            }
            if(prefix.isEmpty())continue;
            auto* form=forms[prefix];QWidget* widget=nullptr;
            const auto label=field["label"].toString(path.mid(1));
            if(type=="number"||type=="integer") {
                auto* spin=new QDoubleSpinBox(properties_);widget=spin;
                spin->setRange(field["min"].toDouble(-1e12),field["max"].toDouble(1e12));
                spin->setDecimals(type=="integer"?0:field["decimals"].toInt(3));spin->setSingleStep(field["step"].toDouble(type=="integer"?1:0.1));spin->setKeyboardTracking(false);
                connect(spin,&QDoubleSpinBox::editingFinished,this,[this,spin,path,type]{SubmitProperty(path,type=="integer"?QJsonValue(static_cast<qint64>(spin->value())):QJsonValue(spin->value()));});
                propertyRefresh_.push_back([spin,path](const auto& data){if(!Editing(spin)){QSignalBlocker block(spin);spin->setValue(Read(data,path).toDouble());}});
            } else if(type=="bool") {
                auto* check=new QCheckBox(properties_);widget=check;
                connect(check,&QCheckBox::clicked,this,[this,check,path]{SubmitProperty(path,check->isChecked());});
                propertyRefresh_.push_back([check,path](const auto& data){QSignalBlocker block(check);check->setChecked(Read(data,path).toBool());});
            } else if(type=="enum"||type=="object") {
                auto* combo=new QComboBox(properties_);widget=combo;
                if(type=="enum")for(const auto& choice:field["options"].toArray()){auto option=choice.toObject();combo->addItem(option["label"].toString(),QVariant::fromValue(option.value("value")));}
                else {
                    combo->addItem(QStringLiteral("Не задано"),QVariant::fromValue(QJsonValue(qint64(0))));
                    const auto required=field["objectType"].toString();
                    for(const auto& row:view.objects)if(required.isEmpty()||required==QString::fromStdString(row.type))combo->addItem(QString::fromStdString(row.name),QVariant::fromValue(QJsonValue(static_cast<qint64>(row.id))));
                }
                connect(combo,qOverload<int>(&QComboBox::activated),this,[this,combo,path](int index){SubmitProperty(path,combo->itemData(index).value<QJsonValue>());});
                propertyRefresh_.push_back([combo,path](const auto& data){QSignalBlocker block(combo);const auto current=Read(data,path);int index=-1;for(int i=0;i<combo->count();++i)if(combo->itemData(i).value<QJsonValue>()==current){index=i;break;}combo->setCurrentIndex(index);});
            } else if(type=="string"||type=="asset") {
                auto* line=new QLineEdit(properties_);widget=line;line->setMaxLength(4096);
                connect(line,&QLineEdit::editingFinished,this,[this,line,path]{SubmitProperty(path,line->text());});
                propertyRefresh_.push_back([line,path](const auto& data){if(!Editing(line)){QSignalBlocker block(line);line->setText(Read(data,path).toString());}});
                if(type=="asset") {
                    line->setPlaceholderText("/Game/…");
                    auto* pick=new QPushButton(QStringLiteral("Выбрать ресурс…"),properties_);
                    connect(pick,&QPushButton::clicked,this,[this,line,path,field] {
                        const auto root=property("contentRoot").toString();if(root.isEmpty())return;
                        QStringList masks;for(const auto& ext:field["extensions"].toArray())masks<<"*"+ext.toString();
                        const auto chosen=QFileDialog::getOpenFileName(this,QStringLiteral("Ресурс проекта"),root,masks.isEmpty()?QString():QString("Ресурсы (%1)").arg(masks.join(' ')));
                        if(chosen.isEmpty())return;const auto relative=QDir(root).relativeFilePath(chosen);
                        if(relative.startsWith("..")||QDir::isAbsolutePath(relative))return;
                        line->setText("/Game/"+QDir::fromNativeSeparators(relative));SubmitProperty(path,line->text());
                    });
                    form->addRow(pick);pick->setEnabled(!field["readOnly"].toBool());
                }
            } else {widget=new QLabel(QStringLiteral("Неподдерживаемый тип: ")+type,properties_);}
            widget->setObjectName("Property"+path);widget->setToolTip(field["description"].toString());widget->setEnabled(!field["readOnly"].toBool());
            form->addRow(label,widget);
        }
    }
    const auto data=QJsonDocument::fromJson(QByteArray::fromStdString(object.projectData)).object();
    for(const auto& refresh:propertyRefresh_)refresh(data);
}
