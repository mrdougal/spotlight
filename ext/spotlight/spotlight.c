/*
 *  spotlight.c
 *  spotlight
 *
 *  Created by xli on 4/26/10.
 *  Copyright 2010 ThoughtWorks. All rights reserved.
 */

#include <ruby.h>
#include <CoreServices/CoreServices.h>

#define RELEASE_IF_NOT_NULL(ref) { if (ref) { CFRelease(ref); } }

void MDItemSetAttribute(MDItemRef item, CFStringRef name, CFTypeRef value);

VALUE method_search(VALUE self, VALUE queryString, VALUE scopeDirectory);
VALUE method_attributes(VALUE self, VALUE path);
VALUE method_set_attribute(VALUE self, VALUE path, VALUE name, VALUE value);

void Init_spotlight (void)
{
  VALUE Spotlight = rb_define_module("Spotlight");
  rb_define_method(Spotlight, "search", method_search, 2);
  rb_define_method(Spotlight, "attributes", method_attributes, 1);
  rb_define_method(Spotlight, "set_attribute", method_set_attribute, 3);
}

VALUE cfstring2rbstr(CFStringRef str) {
  CFIndex len = CFStringGetMaximumSizeForEncoding(CFStringGetLength(str), kCFStringEncodingUTF8);
  char result[len];
  CFStringGetCString(str, result, len, kCFStringEncodingUTF8);
  return rb_str_new2(result);
}

CFStringRef rbstr2cfstring(VALUE str) {
  return CFStringCreateWithCString(kCFAllocatorDefault, StringValuePtr(str), kCFStringEncodingUTF8);
}

CFStringRef date_string(CFDateRef date) {
  CFLocaleRef locale = CFLocaleCopyCurrent();
  CFDateFormatterRef formatter = CFDateFormatterCreate(kCFAllocatorDefault, locale, kCFDateFormatterFullStyle, kCFDateFormatterFullStyle);
  CFStringRef result = CFDateFormatterCreateStringWithDate(kCFAllocatorDefault, formatter, date);
  RELEASE_IF_NOT_NULL(formatter);
  return result;
}

CFStringRef number_string(CFNumberRef number) {
  CFLocaleRef locale = CFLocaleCopyCurrent();
  CFNumberFormatterRef formatter = CFNumberFormatterCreate(kCFAllocatorDefault, locale, kCFNumberFormatterRoundHalfUp);
  CFStringRef result = CFNumberFormatterCreateStringWithNumber(kCFAllocatorDefault, formatter, number);
  RELEASE_IF_NOT_NULL(locale);
  RELEASE_IF_NOT_NULL(formatter);
  return result;
}


VALUE convert2rb_type(CFTypeRef ref) {
  VALUE result = Qnil;
  if (ref != nil) {
    if (CFGetTypeID(ref) == CFStringGetTypeID()) {
      result = cfstring2rbstr(ref);
    } else if (CFGetTypeID(ref) == CFDateGetTypeID()) {
      CFStringRef date_str = date_string(ref);
      result = cfstring2rbstr(date_str);
      result = rb_funcall(result, rb_intern("to_datetime"), 0);
      RELEASE_IF_NOT_NULL(date_str);
    } else if (CFGetTypeID(ref) == CFArrayGetTypeID()) {
      result = rb_ary_new();
      int i;
      for (i = 0; i < CFArrayGetCount(ref); i++) {
        rb_ary_push(result, convert2rb_type(CFArrayGetValueAtIndex(ref, i)));
      }
    } else if (CFGetTypeID(ref) == CFNumberGetTypeID()) {
      CFStringRef number_str = number_string(ref);
      result = cfstring2rbstr(number_str);
      if (CFNumberIsFloatType(ref)) {
        result = rb_funcall(result, rb_intern("to_f"), 0);
      } else {
        result = rb_funcall(result, rb_intern("to_i"), 0);
      }
      RELEASE_IF_NOT_NULL(number_str);
    }
  }
  return result;
}

CFTypeRef convert2cf_type(VALUE obj) {
  CFTypeRef result = nil;
  double double_result;
  int int_result;
  int i, len;
  VALUE tmp[1];

  switch (TYPE(obj)) {
    case T_NIL:
      result = nil;
      break;
    case T_TRUE:
      result = kCFBooleanTrue;
      break;
    case T_FALSE:
      result = kCFBooleanFalse;
      break;
    case T_FLOAT:
      double_result = NUM2DBL(obj);
      result = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &double_result);
      break;
    case T_FIXNUM:
      int_result = NUM2INT(obj);
      result = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &int_result);
      break;
    case T_STRING:
      result = rbstr2cfstring(obj);
      break;
    case T_OBJECT:
      //todo: need handle Date
      break;
    case T_ARRAY:
      len = RARRAY(obj)->len;
      CFTypeRef values[len];
      for (i = 0; i < len; i++) {
        tmp[0] = INT2NUM(i);
        values[i] = convert2cf_type(rb_ary_aref(1, tmp, obj));
      }
      result = CFArrayCreate(kCFAllocatorDefault, values, len, nil);
      RELEASE_IF_NOT_NULL(values);
      break;
  }
  return result;
}

VALUE method_search(VALUE self, VALUE queryString, VALUE scopeDirectory) {
  
  CFStringRef qs = rbstr2cfstring(queryString);
  MDQueryRef query = MDQueryCreate(kCFAllocatorDefault, qs, nil, nil);
  VALUE result = Qnil;
  int i;

  if (query) {
    CFStringRef scopeDirectoryStr = rbstr2cfstring(scopeDirectory);
    CFStringRef scopeDirectoryStrs[1] = {scopeDirectoryStr};
    RELEASE_IF_NOT_NULL(scopeDirectoryStr);

    CFArrayRef scopeDirectories = CFArrayCreate(kCFAllocatorDefault, (const void **)scopeDirectoryStrs, 1, nil);
    MDQuerySetSearchScope(query, scopeDirectories, 0);
    RELEASE_IF_NOT_NULL(scopeDirectories);

    if (MDQueryExecute(query, kMDQuerySynchronous)) {
      result = rb_ary_new();
      for(i = 0; i < MDQueryGetResultCount(query); ++i) {
        MDItemRef item = (MDItemRef) MDQueryGetResultAtIndex(query, i);
        CFStringRef path = MDItemCopyAttribute(item, kMDItemPath);
        rb_ary_push(result, cfstring2rbstr(path));

        RELEASE_IF_NOT_NULL(path);
      }
    }
    RELEASE_IF_NOT_NULL(query);
  }

  RELEASE_IF_NOT_NULL(qs);
  return result;
}

VALUE method_attributes(VALUE self, VALUE path) {
  VALUE result = rb_hash_new();
  MDItemRef mdi = MDItemCreate(kCFAllocatorDefault, rbstr2cfstring(path));
  CFArrayRef attr_names = MDItemCopyAttributeNames(mdi);
  int i;
  for (i = 0; i < CFArrayGetCount(attr_names); i++) {
    CFStringRef attr_name = CFArrayGetValueAtIndex(attr_names, i);
    CFTypeRef attr_value = MDItemCopyAttribute(mdi, attr_name);
    rb_hash_aset(result, cfstring2rbstr(attr_name), convert2rb_type(attr_value));
    RELEASE_IF_NOT_NULL(attr_value);
  }
  
  RELEASE_IF_NOT_NULL(mdi);
  RELEASE_IF_NOT_NULL(attr_names);

  return result;
}

VALUE method_set_attribute(VALUE self, VALUE path, VALUE name, VALUE value) {
  MDItemRef item = MDItemCreate(kCFAllocatorDefault, rbstr2cfstring(path));
  CFStringRef cfName = rbstr2cfstring(name);
  CFTypeRef cfValue = convert2cf_type(value);
  MDItemSetAttribute(item, cfName, cfValue);

  RELEASE_IF_NOT_NULL(cfValue);
  RELEASE_IF_NOT_NULL(cfName);
  RELEASE_IF_NOT_NULL(item);

  return Qtrue;
}


