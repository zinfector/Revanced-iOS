#import "UIKitOperations.h"
#include "IOSObjectArray.h"
#include "java/lang/Boolean.h"
#include "java/lang/Integer.h"
#include "java/lang/Float.h"
#include "java/lang/IllegalStateException.h"
#include "java/lang/UnsupportedOperationException.h"
#include "java/lang/CharSequence.h"
#include "NSString+JavaString.h"
#include <math.h>
#if TARGET_OS_IOS
#include "android/content/Context.h"
#include "android/content/DialogInterface.h"
#include "android/view/View.h"
#include "android/view/ViewGroup.h"
#include "android/view/Window.h"
#include "android/view/WindowManager.h"
#include "android/view/animation/Animation.h"
#include "android/view/animation/AlphaAnimation.h"
#include "android/widget/AbsSeekBar.h"
#include "android/widget/ProgressBar.h"
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
#include "android/graphics/Typeface.h"
#include "android/graphics/Rect.h"
#include "android/graphics/drawable/Drawable.h"
#include "android/graphics/drawable/ShapeDrawable.h"
#include "android/graphics/drawable/shapes/RoundRectShape.h"
#include "IOSPrimitiveArray.h"
#include "java/util/Map.h"
#include "java/util/List.h"
#include "java/lang/Runnable.h"
#include "rv/batch/Events.h"
#include "app/revanced/extension/youtube/sponsorblock/objects/SponsorSegment.h"
#include <objc/runtime.h>

static UIViewController *(^topPresenter)(void);
static UIView *(^playerOverlay)(void);
static NSMapTable *controllers;
static NSMapTable *windowLayouts;
static NSHashTable *allViews;
static NSMutableDictionary<NSString *,UIButton *> *roleButtons;
static const void *ModelKey=&ModelKey;
static const void *ForegroundKey=&ForegroundKey;
@interface RVBDismiss : NSObject <UIAdaptivePresentationControllerDelegate>
@property(nonatomic,strong) AndroidAppDialog *model;
@end
@implementation RVBDismiss
-(void)presentationControllerDidDismiss:(UIPresentationController *)controller{[self.model dismiss];}
@end
static const void *DismissKey=&DismissKey;
void RVBConfigureUIKit(UIViewController *(^p)(void),UIView *(^v)(void)){
 if(!NSThread.isMainThread||!p||!v)@throw [[JavaLangIllegalStateException alloc] initWithNSString:@"Configure UIKit roles on main"];
 topPresenter=[p copy];playerOverlay=[v copy];controllers=[NSMapTable weakToStrongObjectsMapTable];windowLayouts=[NSMapTable weakToStrongObjectsMapTable];allViews=[NSHashTable weakObjectsHashTable];roleButtons=[NSMutableDictionary dictionary];
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
+(void)tapped:(UITapGestureRecognizer *)gesture;
@end
@implementation RVBUIActions
+(void)clicked:(UIControl *)sender{AndroidViewView *model=objc_getAssociatedObject(sender,ModelKey);@try{[model performClick];}@catch(NSException *e){NSLog(@"[RVB UI callback] %@",e);}}
+(void)tapped:(UITapGestureRecognizer *)gesture{[self clicked:(id)gesture.view];}
+(void)slider:(UISlider *)sender{AndroidWidgetSeekBar *model=objc_getAssociatedObject(sender,ModelKey);@try{[model userProgressWithInt:(int32_t)lroundf(sender.value)];}@catch(NSException *e){NSLog(@"[RVB UI callback] %@",e);}}
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
 [allViews addObject:native];
 native.hidden=model->visibility_!=0;native.alpha=model->alpha_;native.userInteractionEnabled=model->enabled_;
 return native;
}
static void setText(UIView *v,NSString *text){if([v isKindOfClass:UIButton.class])[(UIButton *)v setTitle:text forState:UIControlStateNormal];else if([v isKindOfClass:UILabel.class])((UILabel *)v).text=text;else if([v isKindOfClass:UITextField.class])((UITextField *)v).text=text;}
static UIImage *render(AndroidGraphicsDrawableDrawable *drawable,CGSize size){
 [drawable setBoundsWithInt:0 withInt:0 withInt:(int)size.width withInt:(int)size.height];
 UIGraphicsImageRenderer *renderer=[[UIGraphicsImageRenderer alloc] initWithSize:size];
 return [renderer imageWithActions:^(UIGraphicsImageRendererContext *context){AndroidGraphicsCanvas *canvas=[AndroidGraphicsCanvas new];canvas->handle_=[NSValue valueWithPointer:context.CGContext];[drawable drawWithRVBA4326462Canvas:canvas];}];
}
static void layoutChild(UIView *parent,UIView *child,AndroidViewView *model){
 RVBCDBF4C11ViewGroup_LayoutParams *p=model->layout_;child.translatesAutoresizingMaskIntoConstraints=NO;
 if([parent isKindOfClass:UIStackView.class]){
  UIStackView *stack=(id)parent;
  if(p->width_>0)[child.widthAnchor constraintEqualToConstant:p->width_].active=YES;
  if(p->height_>0)[child.heightAnchor constraintEqualToConstant:p->height_].active=YES;
  if(stack.axis==UILayoutConstraintAxisHorizontal&&p->width_==0){[child setContentHuggingPriority:1 forAxis:UILayoutConstraintAxisHorizontal];}
 }else{
  [child.centerXAnchor constraintEqualToAnchor:parent.centerXAnchor].active=YES;
  if(p->width_>0)[child.widthAnchor constraintEqualToConstant:p->width_].active=YES;
  else if(p->width_==-1)[child.widthAnchor constraintEqualToAnchor:parent.widthAnchor].active=YES;
  if(p->height_>0)[child.heightAnchor constraintEqualToConstant:p->height_].active=YES;
  if([model isKindOfClass:AndroidWidgetTextView.class]&&![model isKindOfClass:AndroidWidgetButton.class]) [child.bottomAnchor constraintEqualToAnchor:parent.bottomAnchor].active=YES;
  else [child.centerYAnchor constraintEqualToAnchor:parent.centerYAnchor].active=YES;
 }
 id drawable=objc_getAssociatedObject(child,ForegroundKey);if(drawable&&[child isKindOfClass:UIButton.class])[(UIButton *)child setImage:render(drawable,CGSizeMake(MAX(24,p->width_),MAX(24,p->height_))) forState:UIControlStateNormal];
}
static void showDialog(AndroidAppDialog *d){
 UIViewController *presenter=topPresenter();if(!presenter)@throw [[JavaLangIllegalStateException alloc] initWithNSString:@"No current presenter"];
 while(presenter.presentedViewController)presenter=presenter.presentedViewController;
 if(([d->window_ getAttributes]->flags_&32)!=0){UIViewController *container=[UIViewController new];UIView *content=view(d->content_);container.view=content;UIView *owner=topPresenter().view;content.translatesAutoresizingMaskIntoConstraints=NO;[owner addSubview:content];NSDictionary *layout=[windowLayouts objectForKey:d->window_];CGFloat y=[layout[@"y"] doubleValue],margin=[layout[@"margin"] doubleValue];[NSLayoutConstraint activateConstraints:@[[content.leadingAnchor constraintEqualToAnchor:owner.safeAreaLayoutGuide.leadingAnchor constant:margin],[content.trailingAnchor constraintEqualToAnchor:owner.safeAreaLayoutGuide.trailingAnchor constant:-margin],[content.bottomAnchor constraintEqualToAnchor:owner.safeAreaLayoutGuide.bottomAnchor constant:-y]]];[controllers setObject:container forKey:d];return;}
 if([d isKindOfClass:AndroidAppAlertDialog.class]){
  AndroidAppAlertDialog *a=(id)d;UIAlertController *alert=[UIAlertController alertControllerWithTitle:[a->title_ description] message:[a->message_ description] preferredStyle:UIAlertControllerStyleAlert];
  for(int i=0;a->items_&&i<a->items_->size_;i++){int item=i;[alert addAction:[UIAlertAction actionWithTitle:[arg(a->items_,i) description] style:UIAlertActionStyleDefault handler:^(UIAlertAction *action){[a->itemListener_ onClickWithRVB57FCEF64DialogInterface:a withInt:item];if(!a->singleChoice_)[a dismiss];}]];}
  for(int button=-1;button>=-3;button--){AndroidWidgetButton *b=[a getButtonWithInt:button];if(!b)continue;
   [alert addAction:[UIAlertAction actionWithTitle:[[b getText] description] style:button==-2?UIAlertActionStyleCancel:UIAlertActionStyleDefault handler:^(UIAlertAction *action){[b performClick];}]];}
  [controllers setObject:alert forKey:d];[presenter presentViewController:alert animated:YES completion:nil];return;
 }
 UIViewController *sheet=[UIViewController new];sheet.view.backgroundColor=UIColor.systemBackgroundColor;
 UIScrollView *scroll=[UIScrollView new];scroll.translatesAutoresizingMaskIntoConstraints=NO;[sheet.view addSubview:scroll];
 [NSLayoutConstraint activateConstraints:@[[scroll.leadingAnchor constraintEqualToAnchor:sheet.view.safeAreaLayoutGuide.leadingAnchor],[scroll.trailingAnchor constraintEqualToAnchor:sheet.view.safeAreaLayoutGuide.trailingAnchor],[scroll.topAnchor constraintEqualToAnchor:sheet.view.safeAreaLayoutGuide.topAnchor],[scroll.bottomAnchor constraintEqualToAnchor:sheet.view.safeAreaLayoutGuide.bottomAnchor]]];
 UIView *content=view(d->content_);content.translatesAutoresizingMaskIntoConstraints=NO;[scroll addSubview:content];
 [NSLayoutConstraint activateConstraints:@[[content.leadingAnchor constraintEqualToAnchor:scroll.contentLayoutGuide.leadingAnchor constant:16],[content.trailingAnchor constraintEqualToAnchor:scroll.contentLayoutGuide.trailingAnchor constant:-16],[content.topAnchor constraintEqualToAnchor:scroll.contentLayoutGuide.topAnchor constant:16],[content.bottomAnchor constraintEqualToAnchor:scroll.contentLayoutGuide.bottomAnchor constant:-16],[content.widthAnchor constraintEqualToAnchor:scroll.frameLayoutGuide.widthAnchor constant:-32]]];
 sheet.modalPresentationStyle=UIModalPresentationPageSheet;sheet.modalInPresentation=!d->cancelable_;[controllers setObject:sheet forKey:d];[presenter presentViewController:sheet animated:YES completion:nil];
 RVBDismiss *delegate=[RVBDismiss new];delegate.model=d;objc_setAssociatedObject(sheet,DismissKey,delegate,OBJC_ASSOCIATION_RETAIN_NONATOMIC);sheet.presentationController.delegate=delegate;
}
id RVBUIKit(NSString *op,IOSObjectArray *a,BOOL *handled){
 *handled=YES;
 if([op isEqual:@"locale"])return [NSLocale.currentLocale.localeIdentifier stringByReplacingOccurrencesOfString:@"_" withString:@"-"];
 if([op isEqual:@"application.version"])return [NSBundle.mainBundle objectForInfoDictionaryKey:@"CFBundleShortVersionString"]?:@"";
 if([op isEqual:@"theme.dark"]){mainThread();return JavaLangBoolean_valueOfWithBoolean_(topPresenter().traitCollection.userInterfaceStyle==UIUserInterfaceStyleDark);}
 if([op isEqual:@"theme.background"]||[op isEqual:@"theme.foreground"]){mainThread();UIColor *c=[op hasSuffix:@"background"]?UIColor.systemBackgroundColor:UIColor.labelColor;CGFloat r,g,b,alpha;[[c resolvedColorWithTraitCollection:topPresenter().traitCollection] getRed:&r green:&g blue:&b alpha:&alpha];return JavaLangInteger_valueOfWithInt_((int32_t)((uint32_t)lround(alpha*255)<<24|(uint32_t)lround(r*255)<<16|(uint32_t)lround(g*255)<<8|(uint32_t)lround(b*255)));}
 if([op hasPrefix:@"context."]){mainThread();AndroidContentContext *c=[[AndroidContentContext alloc] init];c->handle_=[op hasSuffix:@"player_overlay"]?playerOverlay():topPresenter();return c;}
 if([op hasPrefix:@"canvas."]){
  AndroidGraphicsCanvas *canvas=arg(a,0);CGContextRef c=[canvas->handle_ pointerValue];if(!c)@throw [[JavaLangIllegalStateException alloc] initWithNSString:@"Canvas context missing"];
  if([op isEqual:@"canvas.save"]){CGContextSaveGState(c);return JavaLangInteger_valueOfWithInt_(1);}
  if([op isEqual:@"canvas.restore"]){CGContextRestoreGState(c);return nil;}
  if([op isEqual:@"canvas.translate"]){CGContextTranslateCTM(c,[arg(a,1) floatValue],[arg(a,2) floatValue]);return nil;}
  AndroidGraphicsPaint *paint=arg(a,5);UIColor *ink=[color(paint->color_) colorWithAlphaComponent:paint->alpha_/255.0];CGContextSetStrokeColorWithColor(c,ink.CGColor);CGContextSetFillColorWithColor(c,ink.CGColor);CGContextSetLineWidth(c,paint->strokeWidth_);
  CGFloat x=[arg(a,1) floatValue],y=[arg(a,2) floatValue],r=[arg(a,3) floatValue],b=[arg(a,4) floatValue];
  if([op isEqual:@"canvas.line"]){CGContextMoveToPoint(c,x,y);CGContextAddLineToPoint(c,r,b);CGContextStrokePath(c);return nil;}
  if([op isEqual:@"canvas.rect"]){CGContextAddRect(c,CGRectMake(x,y,r-x,b-y));NSString *style=[paint->style_ description];CGContextDrawPath(c,[style isEqual:@"STROKE"]?kCGPathStroke:[style isEqual:@"FILL_AND_STROKE"]?kCGPathFillStroke:kCGPathFill);return nil;}
  *handled=NO;return nil;
 }
 if([op isEqual:@"drawable.shape"]){AndroidGraphicsDrawableShapeDrawable *drawable=arg(a,0);AndroidGraphicsCanvas *canvas=arg(a,1);AndroidGraphicsRect *bounds=[drawable getBounds];CGContextRef c=[canvas->handle_ pointerValue];CGRect rect=CGRectMake(bounds->left_,bounds->top_,bounds->right_-bounds->left_,bounds->bottom_-bounds->top_);CGFloat radius=0;if([drawable->shape_ isKindOfClass:AndroidGraphicsDrawableShapesRoundRectShape.class]){AndroidGraphicsDrawableShapesRoundRectShape *round=(id)drawable->shape_;if(round->radii_&&round->radii_->size_)radius=IOSFloatArray_Get(round->radii_,0);}CGContextSetFillColorWithColor(c,color([drawable getPaint]->color_).CGColor);CGContextAddPath(c,[UIBezierPath bezierPathWithRoundedRect:rect cornerRadius:radius].CGPath);CGContextFillPath(c);return nil;}
 if([op isEqual:@"drawable.tint"]){mainThread();id drawable=arg(a,0);UIColor *tint=color([arg(a,1) intValue]);for(UIView *v in allViews){AndroidWidgetAbsSeekBar *model=objc_getAssociatedObject(v,ModelKey);if([model isKindOfClass:AndroidWidgetAbsSeekBar.class]&&[v isKindOfClass:UISlider.class]){if([model getThumb]==drawable)((UISlider *)v).thumbTintColor=tint;if([model getProgressDrawable]==drawable)((UISlider *)v).minimumTrackTintColor=tint;}}return nil;}
 if([op isEqual:@"resource.animation"]){NSString *name=arg(a,0);if([name isEqual:@"fade_in"]||[name isEqual:@"fade_out"])return [[AndroidViewAnimationAlphaAnimation alloc] initWithFloat:[name isEqual:@"fade_in"]?0:1 withFloat:[name isEqual:@"fade_in"]?1:0];*handled=NO;return nil;}
 if([op isEqual:@"window.layout"]){mainThread();[windowLayouts setObject:@{@"gravity":@([arg(a,1) intValue]),@"y":@([arg(a,2) intValue]),@"margin":@([arg(a,3) intValue])} forKey:arg(a,0)];return nil;}
 if([op hasPrefix:@"window."]){mainThread();return nil;} // Source window flags are consumed by the generic dialog provider.
 if([op isEqual:@"toast"]){NSString *message=[arg(a,0) description];dispatch_async(dispatch_get_main_queue(),^{mainThread();UIView *parent=topPresenter().view;UILabel *label=[UILabel new];label.text=message;label.numberOfLines=0;label.textAlignment=NSTextAlignmentCenter;label.textColor=UIColor.whiteColor;label.backgroundColor=[UIColor.blackColor colorWithAlphaComponent:0.85];label.layer.cornerRadius=8;label.clipsToBounds=YES;label.frame=CGRectMake(16,parent.bounds.size.height-110,parent.bounds.size.width-32,70);[parent addSubview:label];dispatch_after(dispatch_time(DISPATCH_TIME_NOW,2*NSEC_PER_SEC),dispatch_get_main_queue(),^{[label removeFromSuperview];});});return nil;}
 if([op isEqual:@"text.measure"]){NSString *text=arg(a,0);return JavaLangFloat_valueOfWithFloat_([text sizeWithAttributes:@{NSFontAttributeName:[UIFont systemFontOfSize:14]}].width);}
 if([op isEqual:@"text.font_metrics"]){UIFont *font=[UIFont systemFontOfSize:14];RVBA4326462Paint_FontMetricsInt *m=[[RVBA4326462Paint_FontMetricsInt alloc] init];m->ascent_=-ceil(font.ascender);m->descent_=-floor(font.descender);m->top_=m->ascent_;m->bottom_=m->descent_;m->leading_=ceil(font.leading);return m;}
 if([op hasPrefix:@"view."]){
  mainThread();AndroidViewView *model=arg(a,0);UIView *v=view(model);IOSObjectArray *args=a->size_==2&&[arg(a,1) isKindOfClass:IOSObjectArray.class]?arg(a,1):nil;
  if([op isEqual:@"view.animation"]){AndroidViewAnimationAnimation *animation=arg(a,1);if(![animation isKindOfClass:AndroidViewAnimationAlphaAnimation.class]){*handled=NO;return nil;}AndroidViewAnimationAlphaAnimation *alpha=(id)animation;v.alpha=alpha->from_;[UIView animateWithDuration:animation->duration_/1000.0 animations:^{v.alpha=alpha->to_;}];return nil;}
  if([op isEqual:@"view.visibility"]){v.hidden=model->visibility_!=0;return nil;}
  if([op isEqual:@"view.alpha"]){v.alpha=model->alpha_;return nil;}
  if([op isEqual:@"view.enabled"]){v.userInteractionEnabled=model->enabled_;if([v isKindOfClass:UIControl.class])((UIControl *)v).enabled=model->enabled_;return nil;}
  if([op isEqual:@"view.text"]){setText(v,[arg(args,0) description]);return nil;}
  if([op isEqual:@"view.text_color"]){UIColor *c=color([arg(args,0) intValue]);if([v isKindOfClass:UILabel.class])((UILabel *)v).textColor=c;else if([v isKindOfClass:UITextField.class])((UITextField *)v).textColor=c;else if([v isKindOfClass:UIButton.class])[(UIButton *)v setTitleColor:c forState:UIControlStateNormal];return nil;}
  if([op isEqual:@"view.text_size"]){UIFont *font=[UIFont systemFontOfSize:[arg(args,0) floatValue]];if([v isKindOfClass:UILabel.class])((UILabel *)v).font=font;else if([v isKindOfClass:UITextField.class])((UITextField *)v).font=font;else if([v isKindOfClass:UIButton.class])((UIButton *)v).titleLabel.font=font;return nil;}
  if([op isEqual:@"view.background_color"]){v.backgroundColor=color([arg(args,0) intValue]);return nil;}
  if([op isEqual:@"view.add"]){AndroidViewView *childModel=arg(args,0);UIView *child=view(childModel);UIView *parent=v;if([model isKindOfClass:AndroidWidgetGridLayout.class]){AndroidWidgetGridLayout *grid=(id)model;UIStackView *stack=(id)v;UIStackView *row=(id)stack.arrangedSubviews.lastObject;if(!row||row.arrangedSubviews.count>=(NSUInteger)MAX(1,grid->columns_)){row=[UIStackView new];row.axis=UILayoutConstraintAxisHorizontal;row.distribution=UIStackViewDistributionFillEqually;row.spacing=8;[stack addArrangedSubview:row];}parent=row;}if([parent isKindOfClass:UIStackView.class])[(UIStackView *)parent addArrangedSubview:child];else [parent addSubview:child];layoutChild(parent,child,childModel);return nil;}
  if([op isEqual:@"view.remove"]){[view(arg(args,0)) removeFromSuperview];return nil;}
  if([op isEqual:@"view.orientation"]){if([v isKindOfClass:UIStackView.class])((UIStackView *)v).axis=[arg(args,0) intValue]?UILayoutConstraintAxisVertical:UILayoutConstraintAxisHorizontal;return nil;}
  if([op isEqual:@"view.maximum"]){if([v isKindOfClass:UISlider.class])((UISlider *)v).maximumValue=[arg(args,0) intValue];return nil;}
  if([op isEqual:@"view.progress"]){if([v isKindOfClass:UISlider.class])((UISlider *)v).value=[arg(args,0) intValue];return nil;}
  if([op isEqual:@"view.click_listener"]){if(![v isKindOfClass:UIButton.class]&&!v.gestureRecognizers.count)[v addGestureRecognizer:[[UITapGestureRecognizer alloc] initWithTarget:RVBUIActions.class action:@selector(tapped:)]];return nil;}
  if([op isEqual:@"view.clickable"]){v.userInteractionEnabled=model->clickable_;return nil;}
  if([op isEqual:@"view.seek_listener"])return nil; // Native targets dispatch the retained source callbacks.
  if([op isEqual:@"view.layout"]){RVBCDBF4C11ViewGroup_LayoutParams *p=model->layout_;if(p->width_>=0){CGRect f=v.frame;f.size.width=p->width_;v.frame=f;}if(p->height_>=0){CGRect f=v.frame;f.size.height=p->height_;v.frame=f;}return nil;}
  if([op isEqual:@"view.padding"]){if([v isKindOfClass:UIStackView.class])((UIStackView *)v).layoutMargins=UIEdgeInsetsMake([arg(args,1) intValue],[arg(args,0) intValue],[arg(args,3) intValue],[arg(args,2) intValue]);return nil;}
  if([op isEqual:@"view.hint"]){if([v isKindOfClass:UITextField.class])((UITextField *)v).placeholder=[arg(args,0) description];return nil;}
  if([op isEqual:@"view.single_line"]){if([v isKindOfClass:UILabel.class])((UILabel *)v).numberOfLines=[arg(args,0) booleanValue]?1:0;return nil;}
  if([op isEqual:@"view.typeface"]){BOOL bold=arg(args,0)==RVBA4326462Typeface_get_DEFAULT_BOLD();UIFont *old=[v isKindOfClass:UIButton.class]?((UIButton *)v).titleLabel.font:[v isKindOfClass:UILabel.class]?((UILabel *)v).font:[v isKindOfClass:UITextField.class]?((UITextField *)v).font:[UIFont systemFontOfSize:14];UIFont *font=[UIFont systemFontOfSize:old.pointSize weight:bold?UIFontWeightBold:UIFontWeightRegular];if([v isKindOfClass:UIButton.class])((UIButton *)v).titleLabel.font=font;else if([v isKindOfClass:UILabel.class])((UILabel *)v).font=font;else if([v isKindOfClass:UITextField.class])((UITextField *)v).font=font;return nil;}
  if([op isEqual:@"view.gravity"]){int g=[arg(args,0) intValue];NSTextAlignment alignment=(g&7)==1?NSTextAlignmentCenter:(g&7)==5?NSTextAlignmentRight:NSTextAlignmentLeft;if([v isKindOfClass:UILabel.class])((UILabel *)v).textAlignment=alignment;else if([v isKindOfClass:UIButton.class])((UIButton *)v).titleLabel.textAlignment=alignment;else if([v isKindOfClass:UITextField.class])((UITextField *)v).textAlignment=alignment;return nil;}
  if([op isEqual:@"view.background"]){id d=arg(args,0);if([d isKindOfClass:AndroidGraphicsDrawableShapeDrawable.class]){AndroidGraphicsDrawableShapeDrawable *s=d;v.backgroundColor=color([s getPaint]->color_);if([s->shape_ isKindOfClass:AndroidGraphicsDrawableShapesRoundRectShape.class]){AndroidGraphicsDrawableShapesRoundRectShape *r=(id)s->shape_;v.layer.cornerRadius=r->radii_&&r->radii_->size_?IOSFloatArray_Get(r->radii_,0):0;v.clipsToBounds=YES;}}else if(d)@throw [[JavaLangUnsupportedOperationException alloc] initWithNSString:@"background drawable"];return nil;}
  if([op isEqual:@"view.foreground"]){objc_setAssociatedObject(v,ForegroundKey,arg(args,0),OBJC_ASSOCIATION_RETAIN_NONATOMIC);return nil;}
  if([op isEqual:@"view.minimum_height"]){[v.heightAnchor constraintGreaterThanOrEqualToConstant:[arg(args,0) intValue]].active=YES;return nil;}
  if([op isEqual:@"view.focusable"]||[op isEqual:@"view.focusable_touch"]){v.userInteractionEnabled=YES;return nil;}
  if([op isEqual:@"view.all_caps"]){if([arg(args,0) booleanValue]&&[v isKindOfClass:UIButton.class])[(UIButton *)v setTitle:[((UIButton *)v).currentTitle uppercaseString] forState:UIControlStateNormal];return nil;}
  if([op isEqual:@"view.grid_columns"]||[op isEqual:@"view.grid_rows"]||[op isEqual:@"view.grid_alignment"]||[op isEqual:@"view.sound"]||[op isEqual:@"view.system_ui"])return nil; // Source model carries these values; native layout/presentation consumes them.
  // Remaining visual details require explicit providers rather than silent success.
  *handled=NO;return nil;
 }
 if([op isEqual:@"dialog.feature"])return JavaLangBoolean_valueOfWithBoolean_(YES);
 if([op isEqual:@"dialog.content"])return nil; // Content belongs to the source dialog model until presentation.
 if([op isEqual:@"dialog.show"]){mainThread();showDialog(arg(a,0));return nil;}
 if([op isEqual:@"dialog.dismiss"]){mainThread();AndroidAppDialog *d=arg(a,0);UIViewController *c=[controllers objectForKey:d];void(^cleanup)(void)=^{if(!c)return;NSMutableArray *pending=[NSMutableArray arrayWithObject:c.view];for(NSUInteger i=0;i<pending.count;i++){UIView *v=pending[i];[pending addObjectsFromArray:v.subviews];AndroidViewView *m=objc_getAssociatedObject(v,ModelKey);if(m)m->handle_=nil;objc_setAssociatedObject(v,ModelKey,nil,OBJC_ASSOCIATION_RETAIN_NONATOMIC);}objc_setAssociatedObject(c,DismissKey,nil,OBJC_ASSOCIATION_RETAIN_NONATOMIC);};if(([d->window_ getAttributes]->flags_&32)!=0){[c.view removeFromSuperview];cleanup();}else [c dismissViewControllerAnimated:YES completion:cleanup];[controllers removeObjectForKey:d];return nil;}
 if([op isEqual:@"roles.hide_all"]){mainThread();for(UIButton *b in roleButtons.allValues)b.hidden=YES;return nil;}
 if([op isEqual:@"role.hide"]){mainThread();roleButtons[arg(a,0)].hidden=YES;return nil;}
 if([op hasPrefix:@"role.button."]){
  mainThread();NSString *role=arg(a,0);UIButton *button=roleButtons[role];
  if([op hasSuffix:@"hide"]){button.hidden=YES;return nil;}
  RVB88F95091SponsorSegment *segment=arg(a,1);
  if(!button){UIView *owner=playerOverlay();if(!owner)@throw [[JavaLangIllegalStateException alloc] initWithNSString:@"Player overlay owner is unavailable"];
   button=[UIButton buttonWithType:UIButtonTypeSystem];button.hidden=YES;button.backgroundColor=UIColor.secondarySystemBackgroundColor;button.layer.cornerRadius=8;roleButtons[role]=button;}
  UIView *owner=playerOverlay();if(!owner)@throw [[JavaLangIllegalStateException alloc] initWithNSString:@"Player overlay owner is unavailable"];if(button.superview!=owner)[owner addSubview:button];button.frame=CGRectMake(16,MAX(16,owner.bounds.size.height-150-([role isEqual:@"sponsor-highlight"]?48:0)),220,44);
  [button setTitle:[segment getSkipButtonText] forState:UIControlStateNormal];if([op hasSuffix:@"show"])button.hidden=NO;
  [button removeTarget:nil action:NULL forControlEvents:UIControlEventAllEvents];
  [button addAction:[UIAction actionWithHandler:^(UIAction *action){RVBEvents_skipSegmentWithRVB88F95091SponsorSegment_(segment);}] forControlEvents:UIControlEventTouchUpInside];return nil;
 }
 *handled=NO;return nil;
}
#else
id RVBUIKit(NSString *op,IOSObjectArray *a,BOOL *handled){*handled=NO;return nil;}
#endif
