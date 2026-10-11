#import "UIKitOperations.h"
#include "IOSObjectArray.h"
#include "java/lang/Boolean.h"
#include "java/lang/Integer.h"
#include "java/lang/Float.h"
#include "java/lang/IllegalStateException.h"
#include "java/lang/UnsupportedOperationException.h"
#include <math.h>
#if TARGET_OS_IOS
#include "android/content/Context.h"
#include "android/content/DialogInterface.h"
#include "android/view/View.h"
#include "android/view/ViewGroup.h"
#include "android/view/Window.h"
#include "android/widget/TextView.h"
#include "android/widget/Button.h"
#include "android/widget/EditText.h"
#include "android/widget/SeekBar.h"
#include "android/widget/LinearLayout.h"
#include "android/widget/GridLayout.h"
#include "android/app/Dialog.h"
#include "android/app/AlertDialog.h"
#include "android/graphics/Paint.h"
#include "android/graphics/Canvas.h"
#include "java/util/Map.h"
#include "java/util/List.h"
#include "java/lang/Runnable.h"
#include "rv/batch/Events.h"
#include "app/revanced/extension/youtube/sponsorblock/objects/SponsorSegment.h"
#include <objc/runtime.h>

static UIViewController *(^topPresenter)(void);
static UIView *(^playerOverlay)(void);
static NSMapTable *controllers;
static NSMutableDictionary<NSString *,UIButton *> *roleButtons;
static const void *ModelKey=&ModelKey;
void RVBConfigureUIKit(UIViewController *(^p)(void),UIView *(^v)(void)){
 if(!NSThread.isMainThread||!p||!v)@throw [[JavaLangIllegalStateException alloc] initWithNSString:@"Configure UIKit roles on main"];
 topPresenter=[p copy];playerOverlay=[v copy];controllers=[NSMapTable weakToStrongObjectsMapTable];roleButtons=[NSMutableDictionary dictionary];
}
static UIColor *color(int32_t c){return [UIColor colorWithRed:((c>>16)&255)/255.0 green:((c>>8)&255)/255.0 blue:(c&255)/255.0 alpha:((uint32_t)c>>24)/255.0];}
static id arg(IOSObjectArray *a,int i){return IOSObjectArray_Get(a,i);}
static void mainThread(void){if(!NSThread.isMainThread)@throw [[JavaLangIllegalStateException alloc] initWithNSString:@"UIKit operation requires main"];
 if(!topPresenter||!playerOverlay)@throw [[JavaLangIllegalStateException alloc] initWithNSString:@"UIKit owners are not configured"];
}
@interface RVBUIActions : NSObject
+(void)clicked:(UIControl *)sender;
+(void)slider:(UISlider *)sender;
+(void)text:(UITextField *)sender;
@end
@implementation RVBUIActions
+(void)clicked:(UIControl *)sender{AndroidViewView *model=objc_getAssociatedObject(sender,ModelKey);[model performClick];}
+(void)slider:(UISlider *)sender{AndroidWidgetSeekBar *model=objc_getAssociatedObject(sender,ModelKey);[model userProgressWithInt:(int32_t)lroundf(sender.value)];}
+(void)text:(UITextField *)sender{AndroidWidgetEditText *model=objc_getAssociatedObject(sender,ModelKey);[model setTextWithJavaLangCharSequence:sender.text?:@""];}
@end
static UIView *view(AndroidViewView *model){
 if(!model)@throw [[JavaLangIllegalStateException alloc] initWithNSString:@"Source view model is unavailable"];
 if([model->handle_ isKindOfClass:UIView.class])return model->handle_;
 UIView *native;
 if([model isKindOfClass:AndroidWidgetButton.class]){UIButton *b=[UIButton buttonWithType:UIButtonTypeSystem];[b addTarget:RVBUIActions.class action:@selector(clicked:) forControlEvents:UIControlEventTouchUpInside];native=b;}
 else if([model isKindOfClass:AndroidWidgetEditText.class]){UITextField *f=[UITextField new];f.borderStyle=UITextBorderStyleRoundedRect;[f addTarget:RVBUIActions.class action:@selector(text:) forControlEvents:UIControlEventEditingChanged];native=f;}
 else if([model isKindOfClass:AndroidWidgetSeekBar.class]){UISlider *s=[UISlider new];[s addTarget:RVBUIActions.class action:@selector(slider:) forControlEvents:UIControlEventValueChanged];native=s;}
 else if([model isKindOfClass:AndroidWidgetTextView.class]){UILabel *l=[UILabel new];l.numberOfLines=0;native=l;}
 else if([model isKindOfClass:AndroidWidgetLinearLayout.class]||[model isKindOfClass:AndroidWidgetGridLayout.class]){UIStackView *s=[UIStackView new];s.axis=UILayoutConstraintAxisVertical;s.spacing=8;s.layoutMarginsRelativeArrangement=YES;native=s;}
 else native=[UIView new];
 model->handle_=native;objc_setAssociatedObject(native,ModelKey,model,OBJC_ASSOCIATION_RETAIN_NONATOMIC);
 native.hidden=model->visibility_!=0;native.alpha=model->alpha_;native.userInteractionEnabled=model->enabled_;
 return native;
}
static void setText(UIView *v,NSString *text){if([v isKindOfClass:UIButton.class])[(UIButton *)v setTitle:text forState:UIControlStateNormal];else if([v isKindOfClass:UILabel.class])((UILabel *)v).text=text;else if([v isKindOfClass:UITextField.class])((UITextField *)v).text=text;}
static void showDialog(AndroidAppDialog *d){
 UIViewController *presenter=topPresenter();if(!presenter)@throw [[JavaLangIllegalStateException alloc] initWithNSString:@"No current presenter"];
 while(presenter.presentedViewController)presenter=presenter.presentedViewController;
 if([d isKindOfClass:AndroidAppAlertDialog.class]){
  AndroidAppAlertDialog *a=(id)d;UIAlertController *alert=[UIAlertController alertControllerWithTitle:[a->title_ description] message:[a->message_ description] preferredStyle:UIAlertControllerStyleAlert];
  for(int i=0;a->items_&&i<a->items_->size_;i++){int item=i;[alert addAction:[UIAlertAction actionWithTitle:[arg(a->items_,i) description] style:UIAlertActionStyleDefault handler:^(UIAlertAction *action){[a->itemListener_ onClickWithRVB57FCEF64DialogInterface:a withInt:item];if(!a->singleChoice_)[a dismiss];}]];}
  for(int button=-1;button>=-3;button--){AndroidWidgetButton *b=[a getButtonWithInt:button];if(!b)continue;
   [alert addAction:[UIAlertAction actionWithTitle:[[b getText] description] style:button==-2?UIAlertActionStyleCancel:UIAlertActionStyleDefault handler:^(UIAlertAction *action){[b performClick];}]];}
  [controllers setObject:alert forKey:d];[presenter presentViewController:alert animated:YES completion:nil];return;
 }
 UIViewController *sheet=[UIViewController new];sheet.view.backgroundColor=UIColor.systemBackgroundColor;
 UIView *content=view(d->content_);content.translatesAutoresizingMaskIntoConstraints=NO;[sheet.view addSubview:content];
 [NSLayoutConstraint activateConstraints:@[[content.leadingAnchor constraintEqualToAnchor:sheet.view.safeAreaLayoutGuide.leadingAnchor constant:16],[content.trailingAnchor constraintEqualToAnchor:sheet.view.safeAreaLayoutGuide.trailingAnchor constant:-16],[content.topAnchor constraintEqualToAnchor:sheet.view.safeAreaLayoutGuide.topAnchor constant:16],[content.bottomAnchor constraintLessThanOrEqualToAnchor:sheet.view.safeAreaLayoutGuide.bottomAnchor constant:-16]]];
 sheet.modalPresentationStyle=UIModalPresentationPageSheet;sheet.modalInPresentation=!d->cancelable_;[controllers setObject:sheet forKey:d];[presenter presentViewController:sheet animated:YES completion:nil];
}
id RVBUIKit(NSString *op,IOSObjectArray *a,BOOL *handled){
 *handled=YES;
 if([op isEqual:@"locale"])return NSLocale.currentLocale.localeIdentifier;
 if([op isEqual:@"application.version"])return [NSBundle.mainBundle objectForInfoDictionaryKey:@"CFBundleShortVersionString"]?:@"";
 if([op isEqual:@"theme.dark"]){mainThread();return JavaLangBoolean_valueOfWithBoolean_(topPresenter().traitCollection.userInterfaceStyle==UIUserInterfaceStyleDark);}
 if([op isEqual:@"theme.background"]||[op isEqual:@"theme.foreground"]){mainThread();UIColor *c=[op hasSuffix:@"background"]?UIColor.systemBackgroundColor:UIColor.labelColor;CGFloat r,g,b,alpha;[[c resolvedColorWithTraitCollection:topPresenter().traitCollection] getRed:&r green:&g blue:&b alpha:&alpha];return JavaLangInteger_valueOfWithInt_((int32_t)((uint32_t)lround(alpha*255)<<24|(uint32_t)lround(r*255)<<16|(uint32_t)lround(g*255)<<8|(uint32_t)lround(b*255)));}
 if([op hasPrefix:@"context."]){mainThread();AndroidContentContext *c=[[AndroidContentContext alloc] init];c->handle_=[op hasSuffix:@"player_overlay"]?playerOverlay():topPresenter();return c;}
 if([op isEqual:@"resource.integer"])return JavaLangInteger_valueOfWithInt_(200);
 if([op isEqual:@"toast"]){NSString *message=[arg(a,0) description];dispatch_async(dispatch_get_main_queue(),^{mainThread();UIView *parent=topPresenter().view;UILabel *label=[UILabel new];label.text=message;label.numberOfLines=0;label.textAlignment=NSTextAlignmentCenter;label.textColor=UIColor.whiteColor;label.backgroundColor=[UIColor.blackColor colorWithAlphaComponent:0.85];label.layer.cornerRadius=8;label.clipsToBounds=YES;label.frame=CGRectMake(16,parent.bounds.size.height-110,parent.bounds.size.width-32,70);[parent addSubview:label];dispatch_after(dispatch_time(DISPATCH_TIME_NOW,2*NSEC_PER_SEC),dispatch_get_main_queue(),^{[label removeFromSuperview];});});return nil;}
 if([op isEqual:@"text.measure"]){NSString *text=arg(a,0);return JavaLangFloat_valueOfWithFloat_([text sizeWithAttributes:@{NSFontAttributeName:[UIFont systemFontOfSize:14]}].width);}
 if([op isEqual:@"text.font_metrics"]){UIFont *font=[UIFont systemFontOfSize:14];RVBA4326462Paint_FontMetricsInt *m=[[RVBA4326462Paint_FontMetricsInt alloc] init];m->ascent_=-ceil(font.ascender);m->descent_=-floor(font.descender);m->top_=m->ascent_;m->bottom_=m->descent_;m->leading_=ceil(font.leading);return m;}
 if([op hasPrefix:@"view."]){
  mainThread();AndroidViewView *model=arg(a,0);UIView *v=view(model);IOSObjectArray *args=a->size_==2&&[arg(a,1) isKindOfClass:IOSObjectArray.class]?arg(a,1):nil;
  if([op isEqual:@"view.visibility"]){v.hidden=model->visibility_!=0;return nil;}
  if([op isEqual:@"view.alpha"]){v.alpha=model->alpha_;return nil;}
  if([op isEqual:@"view.enabled"]){v.userInteractionEnabled=model->enabled_;if([v isKindOfClass:UIControl.class])((UIControl *)v).enabled=model->enabled_;return nil;}
  if([op isEqual:@"view.text"]){setText(v,[arg(args,0) description]);return nil;}
  if([op isEqual:@"view.text_color"]){UIColor *c=color([arg(args,0) intValue]);if([v isKindOfClass:UILabel.class])((UILabel *)v).textColor=c;else if([v isKindOfClass:UITextField.class])((UITextField *)v).textColor=c;else if([v isKindOfClass:UIButton.class])[(UIButton *)v setTitleColor:c forState:UIControlStateNormal];return nil;}
  if([op isEqual:@"view.text_size"]){UIFont *font=[UIFont systemFontOfSize:[arg(args,0) floatValue]];if([v isKindOfClass:UILabel.class])((UILabel *)v).font=font;else if([v isKindOfClass:UITextField.class])((UITextField *)v).font=font;else if([v isKindOfClass:UIButton.class])((UIButton *)v).titleLabel.font=font;return nil;}
  if([op isEqual:@"view.background_color"]){v.backgroundColor=color([arg(args,0) intValue]);return nil;}
  if([op isEqual:@"view.add"]){UIView *child=view(arg(args,0));if([v isKindOfClass:UIStackView.class])[(UIStackView *)v addArrangedSubview:child];else [v addSubview:child];return nil;}
  if([op isEqual:@"view.remove"]){[view(arg(args,0)) removeFromSuperview];return nil;}
  if([op isEqual:@"view.orientation"]){if([v isKindOfClass:UIStackView.class])((UIStackView *)v).axis=[arg(args,0) intValue]?UILayoutConstraintAxisVertical:UILayoutConstraintAxisHorizontal;return nil;}
  if([op isEqual:@"view.maximum"]){if([v isKindOfClass:UISlider.class])((UISlider *)v).maximumValue=[arg(args,0) intValue];return nil;}
  if([op isEqual:@"view.progress"]){if([v isKindOfClass:UISlider.class])((UISlider *)v).value=[arg(args,0) intValue];return nil;}
  if([op isEqual:@"view.click_listener"]||[op isEqual:@"view.seek_listener"])return nil; // Native targets dispatch the retained source callbacks.
  if([op isEqual:@"view.layout"]){RVBCDBF4C11ViewGroup_LayoutParams *p=model->layout_;if(p->width_>=0){CGRect f=v.frame;f.size.width=p->width_;v.frame=f;}if(p->height_>=0){CGRect f=v.frame;f.size.height=p->height_;v.frame=f;}return nil;}
  if([op isEqual:@"view.padding"]){if([v isKindOfClass:UIStackView.class])((UIStackView *)v).layoutMargins=UIEdgeInsetsMake([arg(args,1) intValue],[arg(args,0) intValue],[arg(args,3) intValue],[arg(args,2) intValue]);return nil;}
  if([op isEqual:@"view.hint"]){if([v isKindOfClass:UITextField.class])((UITextField *)v).placeholder=[arg(args,0) description];return nil;}
  if([op isEqual:@"view.single_line"]){if([v isKindOfClass:UILabel.class])((UILabel *)v).numberOfLines=[arg(args,0) booleanValue]?1:0;return nil;}
  // Remaining visual details require explicit providers rather than silent success.
  *handled=NO;return nil;
 }
 if([op isEqual:@"dialog.feature"])return JavaLangBoolean_valueOfWithBoolean_(YES);
 if([op isEqual:@"dialog.content"])return nil; // Content belongs to the source dialog model until presentation.
 if([op isEqual:@"dialog.show"]){mainThread();showDialog(arg(a,0));return nil;}
 if([op isEqual:@"dialog.dismiss"]){mainThread();AndroidAppDialog *d=arg(a,0);UIViewController *c=[controllers objectForKey:d];[c dismissViewControllerAnimated:YES completion:nil];[controllers removeObjectForKey:d];return nil;}
 if([op isEqual:@"roles.hide_all"]){mainThread();for(UIButton *b in roleButtons.allValues)b.hidden=YES;return nil;}
 if([op hasPrefix:@"role.button."]){
  mainThread();NSString *role=arg(a,0);UIButton *button=roleButtons[role];
  if([op hasSuffix:@"hide"]){button.hidden=YES;return nil;}
  RVB88F95091SponsorSegment *segment=arg(a,1);
  if(!button){UIView *owner=playerOverlay();if(!owner)@throw [[JavaLangIllegalStateException alloc] initWithNSString:@"Player overlay owner is unavailable"];
   button=[UIButton buttonWithType:UIButtonTypeSystem];button.backgroundColor=UIColor.secondarySystemBackgroundColor;button.layer.cornerRadius=8;button.frame=CGRectMake(16,MAX(16,owner.bounds.size.height-150),220,44);[owner addSubview:button];roleButtons[role]=button;}
  [button setTitle:[segment getSkipButtonText] forState:UIControlStateNormal];button.hidden=NO;
  [button removeTarget:nil action:NULL forControlEvents:UIControlEventAllEvents];
  [button addAction:[UIAction actionWithHandler:^(UIAction *action){RVBEvents_skipSegmentWithRVB88F95091SponsorSegment_(segment);}] forControlEvents:UIControlEventTouchUpInside];return nil;
 }
 *handled=NO;return nil;
}
#else
id RVBUIKit(NSString *op,IOSObjectArray *a,BOOL *handled){*handled=NO;return nil;}
#endif
