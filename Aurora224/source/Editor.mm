#import <Cocoa/Cocoa.h>
#include "Editor.h"
#include "Reverb.h"
#include "public.sdk/source/common/pluginview.h"
#include "public.sdk/source/vst/vsteditcontroller.h"
#include <array>
#include <cstring>
using namespace Steinberg;
using namespace Steinberg::Vst;
namespace {
NSColor* color(unsigned rgb) { return [NSColor colorWithSRGBRed:((rgb>>16)&255)/255. green:((rgb>>8)&255)/255. blue:(rgb&255)/255. alpha:1.]; }
void box(NSRect r, unsigned fill, CGFloat radius=0) {
    [color(fill) setFill]; [[NSBezierPath bezierPathWithRoundedRect:r xRadius:radius yRadius:radius] fill];
}
void text(NSString* s, NSRect r, CGFloat size, unsigned rgb, bool mono=false, NSTextAlignment align=NSTextAlignmentLeft) {
    NSMutableParagraphStyle* style=[[[NSMutableParagraphStyle alloc] init] autorelease];
    style.alignment=align;
    NSFont* font=mono ? [NSFont monospacedSystemFontOfSize:size weight:NSFontWeightMedium] : [NSFont systemFontOfSize:size weight:NSFontWeightSemibold];
    [s drawInRect:r withAttributes:@{NSFontAttributeName:font,NSForegroundColorAttributeName:color(rgb),NSParagraphStyleAttributeName:style}];
}
struct Preset { const char* name; std::array<double,10> values; };
const Preset presets[]{
    {"MEDIUM RANDOM HALL",{0,.24,.08,.5,.75,.55,.5,.35,1,.3}},
    {"VOCAL SILK",{0,.2393,.14,.48,.82,.65,.48,.30,.88,.18}},
    {"MIDNIGHT SYNTH",{0,.6154,.08,.70,.86,.55,.6,.65,1,.35}},
    {"SNARE PLATE",{.5,.1282,.048,.42,.9,.35,.4,.2,.9,.2}},
    {"SMALL CHAMBER",{1,.0513,.02,.25,.68,.6,.45,.15,.7,.15}},
    {"GOLDEN ROOM",{1,.1453,.06,.4,.7,.48,.6,.25,.85,.25}},
    {"INFINITE VELVET",{0,.9,.16,.9,.9,.74,.8,.55,1,.45}},
    {"BRIGHT PLATE",{.5,.1966,.03,.6,.85,.15,.45,.3,1,.28}},
    {"DARK CATHEDRAL",{0,.7436,.24,.95,.82,.86,.72,.45,.95,.4}},
    {"DRUM AMBIENCE",{1,.0256,.012,.16,.55,.4,.35,.08,.9,.15}}
};
const int mainIDs[]{1,2,3,5,7,9};
const int toneIDs[]{4,6,8,5,7,9};
const char* mainNames[]{"DECAY","PRE DLY","SIZE","DAMP","MOD","MIX"};
const char* toneNames[]{"DIFFUSE","BASS","WIDTH","DAMP","MOD","MIX"};
}

@interface LRButton : NSButton
@property(nonatomic) BOOL blue;
@end
@implementation LRButton
- (void)drawRect:(NSRect)dirty {
    (void)dirty;
    BOOL lit=self.state==NSControlStateValueOn || [self.cell isHighlighted];
    NSRect r=NSInsetRect(self.bounds,1,1);
    box(r,0x766f5d,5); r=NSInsetRect(r,1,1);
    NSColor* top=color(self.blue ? (lit?0x71a3bd:0x41738c) : (lit?0xd3c49b:0xfff9e9));
    NSColor* bottom=color(self.blue ? 0x254354 : (lit?0xb2a477:0xbdb7a6));
    NSGradient* g=[[[NSGradient alloc] initWithStartingColor:top endingColor:bottom] autorelease];
    [g drawInBezierPath:[NSBezierPath bezierPathWithRoundedRect:r xRadius:4 yRadius:4] angle:90];
    text(self.title,NSMakeRect(2,(r.size.height-13)/2, r.size.width,18),11,self.blue?0xf3f0de:0x383b35,false,NSTextAlignmentCenter);
}
@end

@interface LRFader : NSControl {
@public BOOL tracking;
}
@property(nonatomic) double normalized;
@property(nonatomic,assign) EditController* controller;
@end
@implementation LRFader
- (BOOL)isFlipped { return YES; }
- (BOOL)acceptsFirstResponder { return YES; }
- (double)doubleValue { return self.normalized; }
- (void)setDoubleValue:(double)v { self.normalized=std::clamp(v,0.,1.); [self setNeedsDisplay:YES]; }
- (void)drawRect:(NSRect)dirty {
    (void)dirty;
    CGFloat cx=self.bounds.size.width/2, h=self.bounds.size.height;
    for(int i=0;i<=16;++i) {
        CGFloat y=14+(h-28)*i/16.;
        box(NSMakeRect(5,y,13,1),i%4==0?0x69695e:0xa2a092);
        box(NSMakeRect(cx+8,y,13,1),i%4==0?0x69695e:0xa2a092);
    }
    box(NSMakeRect(cx-3,12,6,h-24),0xb7b19e,3);
    box(NSMakeRect(cx-1.5,12,3,h-24),0x32352e,1);
    CGFloat y=14+(1-self.normalized)*(h-28);
    box(NSMakeRect(1,y-8,self.bounds.size.width-2,20),0xaaa38e,3);
    NSGradient* g=[[[NSGradient alloc] initWithStartingColor:color(0xfffce9) endingColor:color(0xc8c0a7)] autorelease];
    [g drawInRect:NSMakeRect(2,y-9,self.bounds.size.width-4,17) angle:90];
    box(NSMakeRect(4,y-1,self.bounds.size.width-8,2),0x8d8874);
}
- (void)updateAt:(NSEvent*)event {
    NSPoint p=[self convertPoint:event.locationInWindow fromView:nil];
    [self setDoubleValue:1.-(p.y-14)/(self.bounds.size.height-28)];
    [self sendAction:self.action to:self.target];
}
- (void)mouseDown:(NSEvent*)event {
    [self.window makeFirstResponder:self];
    tracking=YES; self.controller->beginEdit((ParamID)self.tag);
    if(event.clickCount==2) {
        [self setDoubleValue:aurora::defaults[self.tag]]; [self sendAction:self.action to:self.target];
    } else {
        [self updateAt:event];
        while(true) {
            NSEvent* next=[self.window nextEventMatchingMask:NSEventMaskLeftMouseDragged|NSEventMaskLeftMouseUp];
            if(!next || next.type==NSEventTypeLeftMouseUp) break;
            [self updateAt:next];
        }
    }
    self.controller->endEdit((ParamID)self.tag); tracking=NO;
}
- (void)scrollWheel:(NSEvent*)event {
    [self setDoubleValue:self.normalized+event.scrollingDeltaY*.005];
    [self sendAction:self.action to:self.target];
}
- (void)keyDown:(NSEvent*)event {
    if(event.keyCode==126 || event.keyCode==125) {
        double step=(event.modifierFlags&NSEventModifierFlagShift)? .001:.01;
        [self setDoubleValue:self.normalized+(event.keyCode==126?step:-step)];
        [self sendAction:self.action to:self.target];
    } else [super keyDown:event];
}
@end

@interface LRPanel : NSView {
    EditController* controller;
    NSTimer* timer;
    LRFader* faders[6];
    LRButton* modes[3];
    LRButton* bypass;
    int page;
}
- (id)initWithController:(EditController*)c;
- (void)shutdown;
- (void)refresh:(id)sender;
@end
@implementation LRPanel
- (BOOL)isFlipped { return YES; }
- (LRButton*)button:(NSString*)label rect:(NSRect)r tag:(NSInteger)tag action:(SEL)action blue:(BOOL)blue {
    LRButton* b=[[[LRButton alloc] initWithFrame:r] autorelease];
    b.title=label; b.tag=tag; b.blue=blue; b.target=self; b.action=action;
    b.bordered=NO; [b setAccessibilityLabel:label]; [self addSubview:b]; return b;
}
- (id)initWithController:(EditController*)c {
    if((self=[super initWithFrame:NSMakeRect(0,0,460,820)])) {
        controller=c; controller->addRef(); page=0;
        [self setAccessibilityLabel:@"Larry's Sexy Reverb controls"];
        for(int i=0;i<10;++i) [self button:[NSString stringWithFormat:@"%d",(i+1)%10] rect:NSMakeRect(218+(i%5)*40,211+(i/5)*32,37,29) tag:100+i action:@selector(preset:) blue:NO];
        NSArray* names=@[@"HALL",@"PLATE",@"CHAMBER"];
        for(int i=0;i<3;++i) modes[i]=[self button:names[i] rect:NSMakeRect(30+i*135,287,130,32) tag:200+i action:@selector(mode:) blue:YES];
        bypass=[self button:@"BYPASS" rect:NSMakeRect(30,331,130,30) tag:300 action:@selector(utility:) blue:NO];
        [self button:@"WET 100%" rect:NSMakeRect(165,331,130,30) tag:301 action:@selector(utility:) blue:NO];
        [self button:@"RESET" rect:NSMakeRect(300,331,130,30) tag:302 action:@selector(utility:) blue:NO];
        for(int i=0;i<6;++i) {
            faders[i]=[[[LRFader alloc] initWithFrame:NSMakeRect(33+i*67,467,48,220)] autorelease];
            faders[i].controller=controller; faders[i].tag=mainIDs[i];
            faders[i].target=self; faders[i].action=@selector(fader:);
            [faders[i] setAccessibilityLabel:[NSString stringWithUTF8String:mainNames[i]]];
            [self addSubview:faders[i]];
        }
        [self button:@"< PRESET" rect:NSMakeRect(30,718,96,38) tag:400 action:@selector(navigation:) blue:NO];
        [self button:@"PRESET >" rect:NSMakeRect(131,718,96,38) tag:401 action:@selector(navigation:) blue:NO];
        [self button:@"INIT" rect:NSMakeRect(232,718,96,38) tag:402 action:@selector(navigation:) blue:NO];
        [self button:@"PAGE >" rect:NSMakeRect(333,718,96,38) tag:403 action:@selector(navigation:) blue:NO];
        [self refresh:nil];
        timer=[[NSTimer timerWithTimeInterval:1./30 target:self selector:@selector(refresh:) userInfo:nil repeats:YES] retain];
        [[NSRunLoop mainRunLoop] addTimer:timer forMode:NSRunLoopCommonModes];
    }
    return self;
}
- (void)shutdown { [timer invalidate]; [timer release]; timer=nil; }
- (void)dealloc { [self shutdown]; controller->release(); [super dealloc]; }
- (void)edit:(ParamID)tag value:(double)value {
    controller->beginEdit(tag); controller->setParamNormalized(tag,value);
    controller->performEdit(tag,value); controller->endEdit(tag);
}
- (int)presetIndex {
    for(int i=0;i<10;++i) {
        bool same=true;
        for(int j=0;j<10;++j) if(std::abs(controller->getParamNormalized(j)-presets[i].values[j])>.0001) { same=false; break; }
        if(same) return i;
    }
    return -1;
}
- (void)applyPreset:(int)index {
    for(int j=0;j<10;++j) [self edit:j value:presets[index].values[j]];
    [self refresh:nil];
}
- (void)preset:(LRButton*)sender { [self applyPreset:(int)sender.tag-100]; }
- (void)mode:(LRButton*)sender { [self edit:0 value:(sender.tag-200)*.5]; [self refresh:nil]; }
- (void)utility:(LRButton*)sender {
    if(sender.tag==300) [self edit:10 value:controller->getParamNormalized(10)<.5?1.:0.];
    if(sender.tag==301) [self edit:9 value:1.];
    if(sender.tag==302) for(int i=0;i<10;++i) [self edit:i value:aurora::defaults[i]];
    [self refresh:nil];
}
- (void)navigation:(LRButton*)sender {
    int index=[self presetIndex];
    if(sender.tag==400) [self applyPreset:(index<0?9:(index+9)%10)];
    if(sender.tag==401) [self applyPreset:(index+1)%10];
    if(sender.tag==402) { for(int i=0;i<10;++i) [self edit:i value:aurora::defaults[i]]; [self edit:10 value:0.]; }
    if(sender.tag==403) page=1-page;
    [self refresh:nil];
}
- (void)fader:(LRFader*)sender {
    ParamID id=(ParamID)sender.tag;
    if(!sender->tracking) controller->beginEdit(id);
    controller->setParamNormalized(id,sender.doubleValue); controller->performEdit(id,sender.doubleValue);
    if(!sender->tracking) controller->endEdit(id);
    [self setNeedsDisplay:YES];
}
- (void)refresh:(id)sender {
    (void)sender;
    const int* ids=page?toneIDs:mainIDs;
    const char** names=page?toneNames:mainNames;
    for(int i=0;i<6;++i) if(!faders[i]->tracking) {
        faders[i].tag=ids[i]; [faders[i] setDoubleValue:controller->getParamNormalized(ids[i])];
        [faders[i] setAccessibilityLabel:[NSString stringWithUTF8String:names[i]]];
    }
    int mode=(int)std::round(controller->getParamNormalized(0)*2.);
    for(int i=0;i<3;++i) { modes[i].state=i==mode?NSControlStateValueOn:NSControlStateValueOff; [modes[i] setNeedsDisplay:YES]; }
    bypass.state=controller->getParamNormalized(10)>=.5?NSControlStateValueOn:NSControlStateValueOff;
    [bypass setNeedsDisplay:YES]; [self setNeedsDisplay:YES];
}
- (NSString*)valueFor:(int)id {
    double v=controller->getParamNormalized(id);
    if(id==1) return [NSString stringWithFormat:@"%.2fs",.3+v*11.7];
    if(id==2) return [NSString stringWithFormat:@"%.0fms",v*250];
    return [NSString stringWithFormat:@"%.0f%%",v*100];
}
- (void)drawRect:(NSRect)dirty {
    (void)dirty;
    box(self.bounds,0x8c897b);
    NSGradient* shell=[[[NSGradient alloc] initWithStartingColor:color(0xf9f5e7) endingColor:color(0xc8c5b6)] autorelease];
    [shell drawInBezierPath:[NSBezierPath bezierPathWithRoundedRect:NSInsetRect(self.bounds,5,5) xRadius:15 yRadius:15] angle:0];
    box(NSMakeRect(16,17,428,786),0xa09a84,11);
    box(NSMakeRect(19,20,422,780),0xe6e3d2,9);
    for(NSValue* point in @[[NSValue valueWithPoint:NSMakePoint(24,25)],[NSValue valueWithPoint:NSMakePoint(436,25)],[NSValue valueWithPoint:NSMakePoint(24,795)],[NSValue valueWithPoint:NSMakePoint(436,795)]]) {
        NSPoint p=point.pointValue; box(NSMakeRect(p.x-3,p.y-3,6,6),0x8d8978,3); box(NSMakeRect(p.x-2,p.y,4,1),0x535347);
    }
    box(NSMakeRect(27,31,406,157),0x6c6452,9); box(NSMakeRect(30,34,400,151),0x151613,7); box(NSMakeRect(36,40,388,139),0x080b0a,4);
    int selected=[self presetIndex];
    NSString* title=selected>=0?[NSString stringWithUTF8String:presets[selected].name]:@"USER EDIT";
    text([NSString stringWithFormat:@"> %02d  %@",selected+1,title],NSMakeRect(48,51,365,22),15,0xec4d35,true);
    NSArray* modesText=@[@"CONCERT HALL",@"RANDOM PLATE",@"SMALL CHAMBER"];
    int mode=std::clamp((int)std::round(controller->getParamNormalized(0)*2.),0,2);
    text([@"> " stringByAppendingString:modesText[mode]],NSMakeRect(48,84,350,22),14,0xbf3729,true);
    text(page?@"PAGE 2 / TONE + SPACE":@"PAGE 1 / REVERB",NSMakeRect(48,149,350,20),11,0xb83b2d,true);
    const char* barNames[]{"TAIL","DARK","WET"}; int barIDs[]{1,5,9};
    for(int row=0;row<3;++row) {
        text([NSString stringWithUTF8String:barNames[row]],NSMakeRect(48+row*124,118,42,14),9,0x96705e,true);
        for(int j=0;j<8;++j) box(NSMakeRect(91+row*124+j*7,120,5,7),controller->getParamNormalized(barIDs[row])*8>j?0xc13b2c:0x341d18,1);
    }
    text(@"Larry's",NSMakeRect(30,207,180,35),30,0x3b403a);
    text(@"SEXY REVERB",NSMakeRect(32,244,170,20),13,0x5b5e52,true);
    text(@"TRIAD AUDIO / 224 INSPIRED",NSMakeRect(32,270,370,13),9,0x7e7d6e,true);
    box(NSMakeRect(28,383,404,65),0x555344,5); box(NSMakeRect(31,386,398,59),0x10130f,3);
    const int* ids=page?toneIDs:mainIDs; const char** names=page?toneNames:mainNames;
    for(int i=0;i<6;++i) {
        text([NSString stringWithUTF8String:names[i]],NSMakeRect(34+i*67,397,64,16),10,0xc94b34,true,NSTextAlignmentCenter);
        text([self valueFor:ids[i]],NSMakeRect(34+i*67,419,64,17),12,0xe14b31,true,NSTextAlignmentCenter);
    }
    text(@"+",NSMakeRect(420,461,14,14),10,0x7b7a6c,true);
    text(@"−",NSMakeRect(420,679,14,14),10,0x7b7a6c,true);
    text(@"DOUBLE CLICK FADER TO RESET",NSMakeRect(32,696,395,14),9,0x77786a,true,NSTextAlignmentCenter);
    text(page?@"02  DIFFUSION / BASS / WIDTH":@"01  DECAY / PREDELAY / SIZE",NSMakeRect(30,772,400,16),10,0x77786a,true,NSTextAlignmentCenter);
}
@end

namespace {
class LarryEditor final : public CPluginView {
    EditController* controller;
    LRPanel* panel=nil;
public:
    explicit LarryEditor(EditController* c):controller(c) { rect={0,0,460,820}; controller->addRef(); }
    ~LarryEditor() override { removed(); controller->release(); }
    tresult PLUGIN_API isPlatformTypeSupported(FIDString type) override { return type && !std::strcmp(type,kPlatformTypeNSView)?kResultTrue:kResultFalse; }
    tresult PLUGIN_API attached(void* parent,FIDString type) override {
        if(!parent || isPlatformTypeSupported(type)!=kResultTrue || panel) return kResultFalse;
        panel=[[LRPanel alloc] initWithController:controller];
        [(NSView*)parent addSubview:panel]; systemWindow=parent; return kResultOk;
    }
    tresult PLUGIN_API removed() override {
        if(panel) { [panel shutdown]; [panel removeFromSuperview]; [panel release]; panel=nil; }
        systemWindow=nullptr; return kResultOk;
    }
    tresult PLUGIN_API onSize(ViewRect* size) override {
        if(!size || size->getWidth()!=460 || size->getHeight()!=820) return kResultFalse;
        rect=*size; return kResultOk;
    }
};
}
IPlugView* makeLarryEditor(EditController* controller) { return new LarryEditor(controller); }
