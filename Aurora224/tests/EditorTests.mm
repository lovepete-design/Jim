#import <Cocoa/Cocoa.h>
#include "pluginterfaces/base/ipluginbase.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/gui/iplugview.h"
#include "base/source/fobject.h"
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace Steinberg;
using namespace Steinberg::Vst;
static void require(bool value,const char* error) { if(!value) throw std::runtime_error(error); }
class Handler final : public FObject, public IComponentHandler {
public:
    int begins=0,changes=0,ends=0;
    tresult PLUGIN_API beginEdit(ParamID) override { ++begins; return kResultOk; }
    tresult PLUGIN_API performEdit(ParamID,ParamValue) override { ++changes; return kResultOk; }
    tresult PLUGIN_API endEdit(ParamID) override { ++ends; return kResultOk; }
    tresult PLUGIN_API restartComponent(int32) override { return kResultOk; }
    OBJ_METHODS(Handler,FObject)
    DEFINE_INTERFACES
        DEF_INTERFACE(IComponentHandler)
    END_DEFINE_INTERFACES(FObject)
    REFCOUNT_METHODS(FObject)
};
static void action(NSView* parent,int tag) {
    NSControl* c=(NSControl*)[parent viewWithTag:tag];
    require(c!=nil,"missing editor control");
    [NSApp sendAction:c.action to:c.target from:c];
}
int main(int argc,char** argv) {
    @autoreleasepool {
        try {
            require(argc>=2,"expected plugin bundle path");
            [NSApplication sharedApplication];
            [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
            CFURLRef url=CFURLCreateFromFileSystemRepresentation(nullptr,(const UInt8*)argv[1],std::strlen(argv[1]),true);
            CFBundleRef bundle=CFBundleCreate(nullptr,url); CFRelease(url);
            require(bundle && CFBundleLoadExecutable(bundle),"cannot load plugin bundle");
            using FactoryProc=IPluginFactory* (*)();
            auto getFactory=(FactoryProc)CFBundleGetFunctionPointerForName(bundle,CFSTR("GetPluginFactory"));
            require(getFactory!=nullptr,"missing factory");
            IPluginFactory* factory=getFactory();
            IEditController* controller=nullptr;
            for(int i=0;i<factory->countClasses();++i) {
                PClassInfo info{}; factory->getClassInfo(i,&info);
                if(std::strcmp(info.category,kVstComponentControllerClass)==0)
                    factory->createInstance(info.cid,IEditController::iid,(void**)&controller);
            }
            require(controller!=nullptr,"missing controller");
            require(controller->initialize(nullptr)==kResultOk,"controller init failed");
            Handler* handler=new Handler;
            controller->setComponentHandler(handler);
            NSWindow* window=[[NSWindow alloc] initWithContentRect:NSMakeRect(0,0,460,820) styleMask:NSWindowStyleMaskTitled backing:NSBackingStoreBuffered defer:NO];
            window.releasedWhenClosed=NO;
            window.title=@"Larry's Sexy Reverb";
            for(int cycle=0;cycle<3;++cycle) {
                IPlugView* view=controller->createView(ViewType::kEditor);
                require(view!=nullptr,"custom editor not available");
                require(view->isPlatformTypeSupported(kPlatformTypeNSView)==kResultTrue,"NSView unsupported");
                require(view->isPlatformTypeSupported("invalid")==kResultFalse,"invalid platform accepted");
                ViewRect size{}; view->getSize(&size);
                require(size.getWidth()==460 && size.getHeight()==820,"wrong editor size");
                require(view->attached(window.contentView,kPlatformTypeNSView)==kResultOk,"editor attach failed");
                action(window.contentView,402); // init
                if(cycle==0 && argc>=3) {
                    NSView* panel=window.contentView.subviews.firstObject;
                    [panel displayIfNeeded];
                    NSBitmapImageRep* image=[panel bitmapImageRepForCachingDisplayInRect:panel.bounds];
                    [panel cacheDisplayInRect:panel.bounds toBitmapImageRep:image];
                    NSData* png=[image representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
                    require([png writeToFile:[NSString stringWithUTF8String:argv[2]] atomically:YES],"preview capture failed");
                }
                NSControl* decay=(NSControl*)[window.contentView viewWithTag:1];
                require(decay!=nil,"decay fader missing");
                decay.doubleValue=.61; action(window.contentView,1);
                require(std::abs(controller->getParamNormalized(1)-.61)<.00001,"fader failed to change parameter");
                action(window.contentView,201);
                require(controller->getParamNormalized(0)==.5,"plate button failed");
                action(window.contentView,104);
                require(controller->getParamNormalized(0)==1.,"chamber preset failed");
                action(window.contentView,301);
                require(controller->getParamNormalized(9)==1.,"wet button failed");
                action(window.contentView,300);
                require(controller->getParamNormalized(10)==1.,"bypass failed");
                controller->setParamNormalized(9,.42);
                [[NSRunLoop mainRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:.12]];
                NSControl* mix=(NSControl*)[window.contentView viewWithTag:9];
                require(std::abs(mix.doubleValue-.42)<.00001,"host automation did not update GUI");
                action(window.contentView,403);
                require([window.contentView viewWithTag:4]!=nil && [window.contentView viewWithTag:8]!=nil,"second fader page missing");
                action(window.contentView,402);
                require(controller->getParamNormalized(10)==0.,"init did not clear bypass");
                require(handler->begins==handler->ends && handler->changes>0,"unbalanced automation gestures");
                require(view->removed()==kResultOk,"editor remove failed");
                require(window.contentView.subviews.count==0,"editor leaked attached subview");
                view->release();
            }
            [window close]; [window release];
            controller->setComponentHandler(nullptr); handler->release();
            controller->terminate(); controller->release(); factory->release();
            CFBundleUnloadExecutable(bundle); CFRelease(bundle);
            std::cout<<"PASS: native editor lifecycle x3, faders, preset and mode buttons, bypass, mix, reset, page switching, automation synchronization, and balanced edit gestures\n";
        } catch(const std::exception& e) { std::cerr<<e.what()<<"\n"; return 1; }
    }
}
