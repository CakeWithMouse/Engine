#define NOMINMAX
#include "UIEditorWindow.h"
#include "EditorLocale.h"
#include <Engine/UI/UIMarkup.h>
#include <QTreeWidget>
#include <QSplitter>
#include <QScrollArea>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QToolBar>
#include <QAction>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QMessageBox>
#include <QColorDialog>
#include <QPainter>
#include <QMouseEvent>
#include <QCloseEvent>
#include <QSignalBlocker>
#include <QStatusBar>
#include <algorithm>

namespace {
QString Type(const UIElement* e) {
    return dynamic_cast<const UIPanel*>(e)?"Panel":dynamic_cast<const UILabel*>(e)?"Label":
        dynamic_cast<const UIButton*>(e)?"Button":dynamic_cast<const UIBar*>(e)?"Bar":"Group";
}
glm::vec4* Color(UIElement* e, int which) {
    if(auto* p=dynamic_cast<UIPanel*>(e)) return which==0?&p->Color:which==1?&p->BorderColor:nullptr;
    if(auto* p=dynamic_cast<UILabel*>(e)) return which==0?&p->Color:nullptr;
    if(auto* p=dynamic_cast<UIBar*>(e)) return which==0?&p->FillColor:which==1?&p->TrackColor:nullptr;
    if(auto* p=dynamic_cast<UIButton*>(e)) {
        glm::vec4* colors[]={&p->Color,&p->TextColor,&p->HoverColor,&p->PressedColor,&p->BorderColor};
        return which>=0&&which<5?colors[which]:nullptr;
    }
    return nullptr;
}
}

// Preview consumes the engine's actual Layout/Build output and GDI font atlas.
// Only the final quad compositing is done by Qt; no duplicate layout or widget renderer.
class UICanvas : public QWidget {
public:
    explicit UICanvas(UIEditorWindow* editor) : QWidget(editor), editor_(editor) {
        setObjectName("UICanvas"); setMinimumSize(480,270); setMouseTracking(true);
        Microsoft::WRL::ComPtr<ID3D11Device> device;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
        if(FAILED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)) || !font_.Build(device.Get())) return;
        Microsoft::WRL::ComPtr<ID3D11Resource> resource; font_.GetSRV()->GetResource(&resource);
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture; resource.As(&texture);
        D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.MiscFlags=0;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> staging;
        if(FAILED(device->CreateTexture2D(&desc,nullptr,&staging))) return;
        context->CopyResource(staging.Get(),texture.Get()); D3D11_MAPPED_SUBRESOURCE mapped{};
        if(FAILED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped))) return;
        atlas_=QImage(static_cast<const uchar*>(mapped.pData),desc.Width,desc.Height,mapped.RowPitch,QImage::Format_RGBA8888).copy();
        context->Unmap(staging.Get(),0);
        // Runtime samples the atlas red channel as coverage; GDI does not write alpha.
        for(int y=0;y<atlas_.height();++y)for(int x=0;x<atlas_.width();++x)
            atlas_.setPixelColor(x,y,QColor(255,255,255,atlas_.pixelColor(x,y).red()));
    }
    QSize resolution{1280,720};
    bool Ready() const { return !atlas_.isNull(); }
    void Layout() { if(auto* root=editor_->Root())root->Layout({0,0,float(resolution.width()),float(resolution.height())}); }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);p.fillRect(rect(),QColor("#242a32"));
        scale_=std::min(double(width())/resolution.width(),double(height())/resolution.height());
        p.scale(scale_,scale_);p.fillRect(QRect(QPoint(),resolution),QColor("#3c4550"));
        p.setClipRect(QRect(QPoint(),resolution)); Layout();
        auto* root=editor_->Root();if(!root)return;
        UIDrawList list;list.Font=&font_;list.ScreenWidth=float(resolution.width());list.ScreenHeight=float(resolution.height());root->Build(list);
        for(size_t i=0;i+5<list.Vertices.size();i+=6) {
            const auto& a=list.Vertices[i];const auto& b=list.Vertices[i+5];
            const auto color=QColor::fromRgbF(a.Color.x,a.Color.y,a.Color.z,a.Color.w);
            const QRectF target(a.Position.x,a.Position.y,b.Position.x-a.Position.x,b.Position.y-a.Position.y);
            if(a.Position.z==b.Position.z&&a.Position.w==b.Position.w) {p.fillRect(target,color);continue;}
            if(!tinted_.contains(color.rgba())) {
                QImage tinted(atlas_.size(),QImage::Format_ARGB32_Premultiplied);tinted.fill(color);
                QPainter tint(&tinted);tint.setCompositionMode(QPainter::CompositionMode_DestinationIn);tint.drawImage(0,0,atlas_);tint.end();
                if(tinted_.size()>64)tinted_.clear();tinted_.insert(color.rgba(),tinted);
            }
            const QRectF source(a.Position.z*atlas_.width(),a.Position.w*atlas_.height(),(b.Position.z-a.Position.z)*atlas_.width(),(b.Position.w-a.Position.w)*atlas_.height());
            p.drawImage(target,tinted_[color.rgba()],source);
        }
        if(auto* selected=editor_->Selected()) {
            const auto box=selected->GetBox();p.setPen(QPen(QColor("#4bd5ff"),2.0/scale_,Qt::DashLine));p.setBrush(Qt::NoBrush);p.drawRect(QRectF(box.X,box.Y,box.Width,box.Height));
        }
    }
    UIElement* Hit(UIElement* e,const QPointF& pos) {
        if(!e||!e->bVisible)return nullptr;
        const auto& children=e->GetChildren();for(auto it=children.rbegin();it!=children.rend();++it)if(auto* hit=Hit(it->get(),pos))return hit;
        return e->GetBox().Contains(float(pos.x()),float(pos.y()))?e:nullptr;
    }
    void mousePressEvent(QMouseEvent* event) override {
        if(event->button()!=Qt::LeftButton)return;
        Layout();start_=event->position()/scale_;editor_->Select(Hit(editor_->Root(),start_));
        if(auto* e=editor_->Selected()){
            original_=e->Rect;drag_=e!=editor_->Root();
            if(original_.Width==0)original_.Width=e->GetBox().Width;
            if(original_.Height==0)original_.Height=e->GetBox().Height;
        }
    }
    void mouseMoveEvent(QMouseEvent* event) override {
        if(!drag_||!(event->buttons()&Qt::LeftButton))return;
        auto* e=editor_->Selected();if(!e)return;
        const auto delta=event->position()/scale_-start_;
        const int a=static_cast<int>(original_.Anchor);
        e->Rect.Width=original_.Width;e->Rect.Height=original_.Height;
        e->Rect.X=original_.X+float(delta.x())*(a%3==2?-1:1);
        e->Rect.Y=original_.Y+float(delta.y())*(a/3==2?-1:1);
        editor_->Changed();editor_->Select(e);
    }
    void mouseReleaseEvent(QMouseEvent*) override { drag_=false; }
private:
    UIEditorWindow* editor_;FontAtlas font_;QImage atlas_;QHash<QRgb,QImage> tinted_;
    double scale_=1;bool drag_=false;QPointF start_;UIRect original_;
};

UIEditorWindow::UIEditorWindow(const QString& file,QWidget* parent) : QMainWindow(parent),file_(file) {
    setObjectName("UIEditorWindow");setAttribute(Qt::WA_DeleteOnClose);resize(1200,760);
    auto* tools=addToolBar(QStringLiteral("UI"));
    saveAction_=tools->addAction(Tr("Сохранить", "Save"),this,[this]{Save();});saveAction_->setShortcut(QKeySequence::Save);saveAction_->setObjectName("UISave");
    reloadAction_=tools->addAction(Tr("Перечитать", "Reload"),this,[this]{if(ConfirmClose())Reload();});tools->addSeparator();
    for(const auto& type:{"Panel","Label","Button","Bar"}) {
        auto* action=tools->addAction(QString::fromLatin1(type),this,[this,type]{AddElement(QString::fromLatin1(type));});action->setObjectName("UIAdd"+QString::fromLatin1(type));
    }
    deleteAction_=tools->addAction(Tr("Удалить", "Delete"),this,[this]{DeleteSelected();});deleteAction_->setObjectName("UIDelete");
    auto* size=new QComboBox(this);size->setObjectName("UIPreviewSize");size->addItems({"1280 × 720","1920 × 1080","800 × 600","720 × 1280"});tools->addWidget(size);
    auto* split=new QSplitter(this);setCentralWidget(split);
    tree_=new QTreeWidget(split);tree_->setHeaderLabel(Tr("Иерархия", "Hierarchy"));tree_->setObjectName("UIHierarchy");
    canvas_=new UICanvas(this);split->addWidget(canvas_);
    auto* scroll=new QScrollArea(split);scroll->setWidgetResizable(true);auto* props=new QWidget(scroll);scroll->setWidget(props);auto* form=new QFormLayout(props);
    name_=new QLineEdit(props);name_->setObjectName("UIName");
    labelName_=new QLabel(Tr("Имя", "Name"),props);form->addRow(labelName_,name_);
    text_=new QLineEdit(props);text_->setObjectName("UIText");
    labelText_=new QLabel(Tr("Текст", "Text"),props);form->addRow(labelText_,text_);
    const QString labels[]={"X","Y",Tr("Ширина", "Width"),Tr("Высота", "Height")};
    for(int i=0;i<4;++i){
        rect_[i]=new QDoubleSpinBox(props);rect_[i]->setObjectName(QString("UIRect%1").arg(i));rect_[i]->setRange(i<2?-100000:0,100000);rect_[i]->setDecimals(2);rect_[i]->setKeyboardTracking(false);
        labelRect_[i]=new QLabel(labels[i],props);form->addRow(labelRect_[i],rect_[i]);
        connect(rect_[i],&QDoubleSpinBox::valueChanged,this,[this]{ApplyInspector();});
    }
    anchor_=new QComboBox(props);anchor_->setObjectName("UIAnchor");anchor_->addItems({"TopLeft","TopCenter","TopRight","CenterLeft","Center","CenterRight","BottomLeft","BottomCenter","BottomRight"});
    labelAnchor_=new QLabel(Tr("Привязка", "Anchor"),props);form->addRow(labelAnchor_,anchor_);
    visible_=new QCheckBox(props);visible_->setObjectName("UIVisible");
    labelVisible_=new QLabel(Tr("Видимость", "Visible"),props);form->addRow(labelVisible_,visible_);
    colorProperty_=new QComboBox(props);
    labelColorProperty_=new QLabel(Tr("Свойство цвета", "Color Property"),props);form->addRow(labelColorProperty_,colorProperty_);
    colorBtn_=new QPushButton(Tr("Цвет и прозрачность…", "Color & Opacity…"),props);colorBtn_->setObjectName("UIColor");form->addRow(colorBtn_);
    hintLabel_=new QLabel(Tr("Размер 0 растягивается до края.\nДобавление — внутрь выбранной панели\nили группы. Кнопки получают логику в C++.", "Size 0 stretches to the edge.\nAdding creates elements inside selected panel\nor group. Buttons receive logic in C++."),props);hintLabel_->setWordWrap(true);form->addRow(hintLabel_);
    split->setSizes({220,700,280});split->setStretchFactor(1,1);
    status_=new QLabel(this);status_->setWordWrap(true);statusBar()->addWidget(status_,1);

    connect(size,&QComboBox::currentIndexChanged,this,[this](int index){const QSize sizes[]={{1280,720},{1920,1080},{800,600},{720,1280}};canvas_->resolution=sizes[index];canvas_->update();});
    connect(tree_,&QTreeWidget::currentItemChanged,this,[this](QTreeWidgetItem* item){if(!refreshing_&&item)Select(reinterpret_cast<UIElement*>(item->data(0,Qt::UserRole).value<quintptr>()));});
    connect(name_,&QLineEdit::editingFinished,this,[this]{ApplyInspector();});connect(text_,&QLineEdit::editingFinished,this,[this]{ApplyInspector();});
    connect(name_,&QLineEdit::textEdited,this,[this]{Changed();});
    connect(text_,&QLineEdit::textEdited,this,[this]{Changed();});
    connect(anchor_,&QComboBox::currentIndexChanged,this,[this]{ApplyInspector();});connect(visible_,&QCheckBox::toggled,this,[this]{ApplyInspector();});
    connect(colorBtn_,&QPushButton::clicked,this,[this]{
        auto* c=Color(selected_,colorProperty_->currentIndex());if(!c)return;
        const auto chosen=QColorDialog::getColor(QColor::fromRgbF(c->r,c->g,c->b,c->a),this,Tr("Цвет UI", "UI Color"),QColorDialog::ShowAlphaChannel);
        if(chosen.isValid()){*c=glm::vec4(chosen.redF(),chosen.greenF(),chosen.blueF(),chosen.alphaF());Changed();}
    });
    Reload();if(!canvas_->Ready())status_->setText(Tr("Не удалось создать атлас шрифта для предпросмотра.", "Failed to create font atlas for preview."));
    langSubId_ = EditorLocale::Instance().SubscribeLanguageChanged([this] {
        RetranslateUi();
    });
}

UIEditorWindow::~UIEditorWindow() {
    // A focused line edit can emit editingFinished while QMainWindow destroys
    // its children, after our root_/selected_ members have already been freed.
    // Teardown must never re-enter the inspector or rebuild its tree.
    refreshing_ = true;
    selected_ = nullptr;
    for (auto* child : findChildren<QObject*>())
        QObject::disconnect(child, nullptr, this, nullptr);
    if (langSubId_) {
        EditorLocale::Instance().UnsubscribeLanguageChanged(langSubId_);
        langSubId_ = 0;
    }
}

void UIEditorWindow::RetranslateUi() {
    setWindowTitle((dirty_ ? QStringLiteral("* ") : QString()) + Tr("Редактор UI — ", "UI Editor — ") + QFileInfo(file_).fileName());
    if (saveAction_) saveAction_->setText(Tr("Сохранить", "Save"));
    if (reloadAction_) reloadAction_->setText(Tr("Перечитать", "Reload"));
    if (deleteAction_) deleteAction_->setText(Tr("Удалить", "Delete"));
    if (tree_) tree_->setHeaderLabel(Tr("Иерархия", "Hierarchy"));
    if (labelName_) labelName_->setText(Tr("Имя", "Name"));
    if (labelText_) labelText_->setText(Tr("Текст", "Text"));
    const QString labels[] = {"X", "Y", Tr("Ширина", "Width"), Tr("Высота", "Height")};
    for (int i = 0; i < 4; ++i) {
        if (labelRect_[i]) labelRect_[i]->setText(labels[i]);
    }
    if (labelAnchor_) labelAnchor_->setText(Tr("Привязка", "Anchor"));
    if (labelVisible_) labelVisible_->setText(Tr("Видимость", "Visible"));
    if (labelColorProperty_) labelColorProperty_->setText(Tr("Свойство цвета", "Color Property"));
    if (colorBtn_) colorBtn_->setText(Tr("Цвет и прозрачность…", "Color & Opacity…"));
    if (hintLabel_) hintLabel_->setText(Tr("Размер 0 растягивается до края.\nДобавление — внутрь выбранной панели\nили группы. Кнопки получают логику в C++.", "Size 0 stretches to the edge.\nAdding creates elements inside selected panel\nor group. Buttons receive logic in C++."));
    RefreshInspector();
}

bool UIEditorWindow::Reload() {
    std::unique_ptr<UIElement> next;std::string error;std::vector<std::string> warnings;
    if(!UIMarkup::Load(file_.toUtf8().toStdString(),next,error,&warnings)){status_->setText(QString::fromStdString(error));return false;}
    root_=std::move(next);selected_=root_.get();dirty_=false;setWindowTitle(Tr("Редактор UI — ", "UI Editor — ")+QFileInfo(file_).fileName());
    RebuildTree();RefreshInspector();canvas_->update();status_->setText(warnings.empty()?file_:QString::fromStdString(warnings.front()));return true;
}
bool UIEditorWindow::Save() {
    if(!root_)return false;
    if(!ApplyInspector())return false;
    const auto text=UIMarkup::Serialize(*root_);std::unique_ptr<UIElement> check;std::string error;
    if(!UIMarkup::Parse(text,file_.toStdString(),check,error)){status_->setText(QString::fromStdString(error));return false;}
    QSaveFile file(file_);if(!file.open(QIODevice::WriteOnly)||file.write(text.data(),qint64(text.size()))!=qint64(text.size())||!file.commit()){status_->setText(file.errorString());return false;}
    dirty_=false;setWindowTitle(Tr("Редактор UI — ", "UI Editor — ")+QFileInfo(file_).fileName());status_->setText(Tr("Сохранено: ", "Saved: ")+file_);return true;
}
void UIEditorWindow::Changed(){dirty_=true;setWindowTitle(Tr("Редактор UI — ", "UI Editor — ")+QFileInfo(file_).fileName()+" *");canvas_->update();}
void UIEditorWindow::RebuildTree(){
    refreshing_=true;tree_->clear();
    std::function<void(UIElement*,QTreeWidgetItem*)> add=[&](UIElement* e,QTreeWidgetItem* parent){
        auto* item=parent?new QTreeWidgetItem(parent):new QTreeWidgetItem(tree_);
        item->setText(0,Type(e)+"  "+QString::fromStdString(e->Name));item->setData(0,Qt::UserRole,QVariant::fromValue(reinterpret_cast<quintptr>(e)));
        if(e==selected_)tree_->setCurrentItem(item);for(const auto& child:e->GetChildren())add(child.get(),item);
    };
    if(root_)add(root_.get(),nullptr);tree_->expandAll();refreshing_=false;
}
void UIEditorWindow::Select(UIElement* element){selected_=element;RefreshInspector();refreshing_=true;QTreeWidgetItemIterator it(tree_);while(*it){if((*it)->data(0,Qt::UserRole).value<quintptr>()==reinterpret_cast<quintptr>(element)){tree_->setCurrentItem(*it);break;}++it;}refreshing_=false;canvas_->update();}
void UIEditorWindow::RefreshInspector(){
    refreshing_=true;const bool valid=selected_!=nullptr;
    for(auto* field:rect_)field->setEnabled(valid);name_->setEnabled(valid);anchor_->setEnabled(valid);visible_->setEnabled(valid);text_->setEnabled(false);colorProperty_->clear();
    if(valid){name_->setText(QString::fromStdString(selected_->Name));const auto& r=selected_->Rect;const float values[]={r.X,r.Y,r.Width,r.Height};for(int i=0;i<4;++i)rect_[i]->setValue(values[i]);anchor_->setCurrentIndex(int(r.Anchor));visible_->setChecked(selected_->bVisible);text_->clear();
        if(auto* p=dynamic_cast<UILabel*>(selected_)){text_->setEnabled(true);text_->setText(QString::fromStdString(p->GetText()));colorProperty_->addItems({Tr("Текст", "Text")});}
        if(auto* p=dynamic_cast<UIButton*>(selected_)){text_->setEnabled(true);text_->setText(QString::fromStdString(p->GetText()));colorProperty_->addItems({Tr("Фон", "Background"),Tr("Текст", "Text"),Tr("Наведение", "Hover"),Tr("Нажатие", "Pressed"),Tr("Рамка", "Border")});}
        if(dynamic_cast<UIPanel*>(selected_))colorProperty_->addItems({Tr("Фон", "Background"),Tr("Рамка", "Border")});
        if(dynamic_cast<UIBar*>(selected_))colorProperty_->addItems({Tr("Заполнение", "Fill"),Tr("Фон", "Background")});
    }refreshing_=false;
}
bool UIEditorWindow::ApplyInspector(){
    if(refreshing_||!selected_)return true;
    const auto name=name_->text().toUtf8().toStdString();auto* existing=root_->Find(name);
    if(!name.empty()&&existing&&existing!=selected_){status_->setText(Tr("Имя уже используется: ", "Name already in use: ")+name_->text());return false;}
    const auto before=UIMarkup::Serialize(*selected_);
    selected_->Name=name;selected_->Rect={UIAnchor(anchor_->currentIndex()),float(rect_[0]->value()),float(rect_[1]->value()),float(rect_[2]->value()),float(rect_[3]->value())};selected_->bVisible=visible_->isChecked();
    if(auto* p=dynamic_cast<UILabel*>(selected_))p->SetText(text_->text().toUtf8().toStdString());if(auto* p=dynamic_cast<UIButton*>(selected_))p->SetText(text_->text().toUtf8().toStdString());
    if(before!=UIMarkup::Serialize(*selected_)){Changed();RebuildTree();}
    return true;
}
void UIEditorWindow::AddElement(const QString& type){
    if(!root_)return;std::unique_ptr<UIElement> next;
    if(type=="Panel")next=std::make_unique<UIPanel>();else if(type=="Label"){auto p=std::make_unique<UILabel>();p->SetText(Tr("Текст", "Text").toUtf8().toStdString());next=std::move(p);}else if(type=="Button"){auto p=std::make_unique<UIButton>();p->SetText(Tr("Кнопка", "Button").toUtf8().toStdString());next=std::move(p);}else if(type=="Bar"){auto p=std::make_unique<UIBar>();p->Fill=0.5f;next=std::move(p);}else return;
    int number=1;do{next->Name=type.toLower().toStdString()+std::to_string(number++);}while(root_->Find(next->Name));next->Rect={UIAnchor::TopLeft,16,16,type=="Panel"?320.0f:200.0f,type=="Panel"?180.0f:36.0f};
    auto* parent=selected_?selected_:root_.get();while(parent!=root_.get()&&Type(parent)!="Panel"&&Type(parent)!="Group")parent=parent->GetParent();selected_=parent->Add(std::move(next));Changed();RebuildTree();RefreshInspector();
}
void UIEditorWindow::DeleteSelected(){if(!selected_||selected_==root_.get())return;auto* parent=selected_->GetParent();parent->RemoveChild(selected_);selected_=parent;Changed();RebuildTree();RefreshInspector();}
bool UIEditorWindow::ConfirmClose(){if(!dirty_)return true;const auto choice=QMessageBox::question(this,Tr("Редактор UI", "UI Editor"),Tr("Сохранить изменения в ", "Save changes to ")+QFileInfo(file_).fileName()+"?",QMessageBox::Save|QMessageBox::Discard|QMessageBox::Cancel);return choice==QMessageBox::Discard||(choice==QMessageBox::Save&&Save());}
void UIEditorWindow::closeEvent(QCloseEvent* event){if(ConfirmClose())event->accept();else event->ignore();}
