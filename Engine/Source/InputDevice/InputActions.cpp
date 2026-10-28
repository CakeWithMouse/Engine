#include <Engine/InputDevice/InputActions.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <set>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace Input
{
struct Connection::Slot
{
    std::weak_ptr<Scope::State> owner;
    Context context;
    std::string action;
    Phase phase;
    std::function<void(const Event&)> callback;
    bool connected = true, active = false;
    unsigned generation = 0;
};
Connection::~Connection() { Disconnect(); }
Connection::Connection(Connection&& other) noexcept : slot_(std::move(other.slot_)) {}
Connection& Connection::operator=(Connection&& other) noexcept
{ if (this != &other) { Disconnect(); slot_ = std::move(other.slot_); } return *this; }
void Connection::Disconnect() { if (slot_) slot_->connected = false; slot_.reset(); }
struct System::Impl
{
    std::vector<Binding> bindings;
    std::vector<std::weak_ptr<Connection::Slot>> slots;
    std::set<Keys> physical, down, blocked;
    std::vector<std::pair<Keys,bool>> edges;
    std::array<bool,3> enabled{true,false,false};
    std::array<int,3> priority{0,100,200};
    float x=0,y=0,wheel=0;
    unsigned epoch=0;
    bool ticking=false;
    void Dispatch(bool frame, float dt)
    {
        const auto revision=epoch;
        std::vector<Binding> candidates;
        for (const auto& b : bindings)
            if (enabled[size_t(b.context)] && std::all_of(b.chord.begin(),b.chord.end(),[&](Keys k){return down.count(k)&&!blocked.count(k);}))
                candidates.push_back(b);
        std::stable_sort(candidates.begin(),candidates.end(),[&](const Binding& a,const Binding& b){
            return std::make_tuple(priority[size_t(a.context)],a.priority,a.chord.size()) >
                std::make_tuple(priority[size_t(b.context)],b.priority,b.chord.size());
        });
        std::set<Keys> consumed;
        std::set<Source> pointers;
        std::map<std::pair<Context,std::string>,float> values;
        for (const auto& b : candidates)
        {
            if (std::any_of(b.chord.begin(),b.chord.end(),[&](Keys k){return consumed.count(k)!=0;}) ||
                (b.source!=Source::Keys && pointers.count(b.source))) continue;
            float value=b.source==Source::Keys ? 1 : !frame ? 0 : b.source==Source::MouseX ? x : b.source==Source::MouseY ? y : wheel;
            values[{b.context,b.action}]+=value*b.scale;
            if (b.consume) { if(b.source==Source::Keys) consumed.insert(b.chord.begin(),b.chord.end()); else pointers.insert(b.source); }
        }
        auto snapshot=slots;
        for (auto& weak : snapshot)
        {
            auto s=weak.lock(); if(!s||!s->connected)continue;
            auto owner=s->owner.lock(); if(!owner||!owner->enabled){s->active=false;continue;}
            if(s->generation!=owner->generation){s->active=false;s->generation=owner->generation;}
            const float v=values[{s->context,s->action}]; const bool active=v!=0;
            const bool call=(s->phase==Phase::Pressed&&active&&!s->active)||
                (s->phase==Phase::Released&&!active&&s->active)||
                (frame&&enabled[size_t(s->context)]&&((s->phase==Phase::Held&&active)||s->phase==Phase::Axis));
            s->active=active;
            if(call)s->callback({v,dt,false});
            if(revision!=epoch)break; // callbacks may rebind, reset or change contexts
        }
        slots.erase(std::remove_if(slots.begin(),slots.end(),[](auto& w){auto s=w.lock();return !s||!s->connected||s->owner.expired();}),slots.end());
    }
};
System::System():impl_(std::make_unique<Impl>()){}
System::~System()=default;
Connection System::Subscribe(Scope& owner, Context context, std::string action, Phase phase, std::function<void(const Event&)> callback)
{
    if(action.empty()||!callback)throw std::invalid_argument("Input subscription needs action and handler");
    auto s=std::make_shared<Connection::Slot>();s->owner=owner.state_;s->generation=owner.state_->generation;
    s->context=context;s->action=std::move(action);s->phase=phase;s->callback=std::move(callback);
    impl_->slots.push_back(s);return Connection(s);
}
void System::Bind(Binding b)
{
    if(b.action.empty()||!std::isfinite(b.scale)||(b.source==Source::Keys&&b.chord.empty()))throw std::invalid_argument("Invalid input binding");
    std::sort(b.chord.begin(),b.chord.end());b.chord.erase(std::unique(b.chord.begin(),b.chord.end()),b.chord.end());
    Reset();impl_->bindings.push_back(std::move(b));
}
void System::Unbind(Context c,const std::string& a){Reset();auto& b=impl_->bindings;b.erase(std::remove_if(b.begin(),b.end(),[&](auto& v){return v.context==c&&v.action==a;}),b.end());}
void System::ClearBindings(Context c){Reset();auto& b=impl_->bindings;b.erase(std::remove_if(b.begin(),b.end(),[&](auto& v){return v.context==c;}),b.end());}
void System::SetContextEnabled(Context c,bool v){if(impl_->enabled[size_t(c)]!=v){Reset();impl_->enabled[size_t(c)]=v;}}
bool System::IsContextEnabled(Context c)const{return impl_->enabled[size_t(c)];}
void System::SetContextPriority(Context c,int v){Reset();impl_->priority[size_t(c)]=v;}
void System::SetKey(Keys k,bool v)
{
    if(k==Keys::None)return;
    if(v){if(!impl_->physical.insert(k).second)return;}
    else {if(!impl_->physical.erase(k))return;}
    impl_->edges.emplace_back(k,v);
    auto alias=[&](Keys left,Keys right,Keys generic){if(k==left||k==right)SetKey(generic,impl_->physical.count(left)||impl_->physical.count(right));};
    alias(Keys::LeftShift,Keys::RightShift,Keys::Shift);alias(Keys::LeftControl,Keys::RightControl,Keys::Control);alias(Keys::LeftAlt,Keys::RightAlt,Keys::Alt);
}
void System::AddPointer(float x,float y,float w){if(std::isfinite(x)&&std::isfinite(y)&&std::isfinite(w)){impl_->x+=x;impl_->y+=y;impl_->wheel+=w;}}
void System::Tick(float dt)
{
    if(impl_->ticking)throw std::logic_error("Recursive input dispatch");
    if(!std::isfinite(dt)||dt<0)throw std::invalid_argument("Invalid input delta time");
    impl_->ticking=true;
    try {
        auto edges=std::move(impl_->edges);impl_->edges.clear();auto epoch=impl_->epoch;
        for(auto [key,down]:edges){if(down)impl_->down.insert(key);else{impl_->down.erase(key);impl_->blocked.erase(key);}impl_->Dispatch(false,dt);if(epoch!=impl_->epoch)break;}
        impl_->Dispatch(true,dt);
    }catch(...){impl_->ticking=false;impl_->x=impl_->y=impl_->wheel=0;throw;}
    impl_->x=impl_->y=impl_->wheel=0;impl_->ticking=false;
}
void System::Reset()
{
    ++impl_->epoch;impl_->edges.clear();impl_->down=impl_->physical;impl_->blocked=impl_->physical;
    impl_->x=impl_->y=impl_->wheel=0;
    auto snapshot=impl_->slots;
    for(auto& w:snapshot){auto s=w.lock();if(!s)continue;bool was=s->active;s->active=false;
        auto owner=s->owner.lock();if(was&&s->connected&&owner&&owner->enabled&&(s->phase==Phase::Released||s->phase==Phase::Axis))s->callback({0,0,true});}
}
void System::ReleaseAll(){Reset();impl_->physical.clear();impl_->down.clear();impl_->blocked.clear();}
}
