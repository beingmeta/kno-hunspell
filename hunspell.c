/* -*- Mode: C; Character-encoding: utf-8; -*- */

/* Copyright (C) 2012-2019 beingmeta, inc.
   Copyright (C) 2020-2022 beingmeta, LLC
   This file is part of beingmeta's Kno platform and is copyright
   and a valuable trade secret of beingmeta, inc.
*/

#ifndef _FILEINFO
#define _FILEINFO __FILE__
#endif

#include "kno/knosource.h"
#include "kno/lisp.h"
#include "kno/eval.h"
#include "kno/storage.h"
#include "kno/pools.h"
#include "kno/indexes.h"
#include "kno/frames.h"
#include "kno/numbers.h"
#include "kno/cprims.h"

#include <libu8/libu8io.h>
#include <libu8/u8pathfns.h>
#include <libu8/u8filefns.h>
#include <hyphen.h>

#define EDGE_LEN 1

KNO_EXPORT int kno_init_hyphenate(void) KNO_LIBINIT_FN;

static u8_string default_hyphenation_file = NULL;
static HyphenDict *default_dict = NULL;

static long long int hyphenate_init = 0;


DEFC_PRIM("hyphenate-word",hyphenate_word_prim,
	  KNO_MAX_ARGS(1)|KNO_MIN_ARGS(1),
	  "returns a copy of *word* with hyphens inserted",
	  {"word",kno_string_type,KNO_VOID})
static lispval hyphenate_word_prim(lispval word)
{
  u8_string string = KNO_CSTRING(word);
  int len = KNO_STRLEN(word);
  if (len==0)
    return kno_incref(word);
  else {
    char *hword = u8_malloc(len*2), *hyphens = u8_malloc(len+5);
    int *pos = NULL, *cut = NULL;
    char ** rep = NULL;
    int retval = hnj_hyphen_hyphenate2
      (default_dict,string,len,hyphens,hword,&rep,&pos,&cut);
    if (retval) {
      u8_free(hyphens);
      u8_free(hword);
      kno_seterr("Hyphenation error","get_hyphen_breaks",string,word);
      return KNO_ERROR_VALUE;}
    else {
      lispval result = kno_make_string(NULL,-1,(u8_string)hword);
      u8_free(hyphens);
      u8_free(hword);
      return result;}}
}


DEFC_PRIM("hyphen-breaks",hyphen_breaks_prim,
	  KNO_MAX_ARGS(1)|KNO_MIN_ARGS(1),
	  "returns the locations in *string* where hyphens may be placed",
	  {"string",kno_string_type,KNO_VOID})
static lispval hyphen_breaks_prim(lispval string)
{
  u8_string s = KNO_CSTRING(string);
  int len = KNO_STRLEN(string);
  if (len==0) return kno_empty_vector(0);
  else {
    char *hyphens = u8_malloc(len+5);
    char ** rep = NULL; int * pos = NULL; int * cut = NULL;
    int retval = hnj_hyphen_hyphenate2
      (default_dict,s,len,hyphens,NULL,&rep,&pos,&cut);
    if (retval) {
      u8_free(hyphens);
      kno_seterr("Hyphenation error","get_hyphen_breaks",s,string);
      return KNO_ERROR_VALUE;}
    else {
      struct U8_OUTPUT out;
      const u8_byte *scan = s;
      lispval result = KNO_VOID, *elts;
      int i = 0, n_breaks = 0, seg = 0, cpos = 0, c = u8_sgetc(&scan);
      while (i<len) {
	if ((hyphens[i++])&(1)) n_breaks++;}
      result = kno_empty_vector(n_breaks+1);
      elts = KNO_VECTOR_DATA(result);
      U8_INIT_OUTPUT(&out,32);
      while (c>=0) {
	u8_putc(&out,c);
	if (hyphens[cpos]&1) {
	  lispval s = kno_make_string
	    (NULL,out.u8_write-out.u8_outbuf,out.u8_outbuf);
	  elts[seg++]=s;
	  out.u8_write = out.u8_outbuf;}
	cpos = scan-s;
	c = u8_sgetc(&scan);}
      if (out.u8_write!=out.u8_outbuf) {
	lispval s = kno_make_string
	  (NULL,out.u8_write-out.u8_outbuf,out.u8_outbuf);
	elts[seg++]=s;
	out.u8_write = out.u8_outbuf;}
      u8_free(out.u8_outbuf);
      return result;}}
}

DEFC_PRIM("shyphenate",shyphenate_prim,
	  KNO_MAX_ARGS(1)|KNO_MIN_ARGS(1),
	  "**undocumented**",
	  {"string",kno_string_type,KNO_VOID})
static lispval shyphenate_prim(lispval string)
{
  u8_string s = KNO_CSTRING(string);
  int len = KNO_STRLEN(string);
  if (len==0) return kno_incref(string);
  else {
    char *hyphens = u8_malloc(len+5);
    char ** rep = NULL; int * pos = NULL; int * cut = NULL;
    int retval = hnj_hyphen_hyphenate2
      (default_dict,s,len,hyphens,NULL,&rep,&pos,&cut);
    if (retval) {
      u8_free(hyphens);
      kno_seterr("Hyphenation error","get_hyphen_breaks",s,string);
      return KNO_ERROR_VALUE;}
    else {
      lispval result = KNO_VOID;
      const u8_byte *scan = s;
      struct U8_OUTPUT out;
      int cpos = 0, c = u8_sgetc(&scan);
      U8_INIT_OUTPUT(&out,len*3);
      while (c>=0) {
	u8_putc(&out,c);
	if (hyphens[cpos]&1) u8_putc(&out,0xAD);
	cpos = scan-s;
	c = u8_sgetc(&scan);}
      result = kno_make_string
	(NULL,out.u8_write-out.u8_outbuf,out.u8_outbuf);
      u8_free(out.u8_outbuf);
      return result;}}
}

static int hyphenout_helper(U8_OUTPUT *out,
			    u8_string string,int len,
			    int hyphen_char)
{
  char *hyphens = u8_malloc(len+5);
  char **rep = NULL; int *pos = NULL; int *cut = NULL;
  int retval = hnj_hyphen_hyphenate2
    (default_dict,string,len,hyphens,NULL,&rep,&pos,&cut);
  if (retval) {
    u8_free(hyphens);
    return -1;}
  else {
    const u8_byte *scan = string;
    int cpos = 0, c = u8_sgetc(&scan), n_hyphens = 0;
    while (c>=0) {
      u8_putc(out,c);
      if ((cpos>EDGE_LEN)&&(hyphens[cpos]&1)&&(cpos<(len-EDGE_LEN))) {
	u8_putc(out,hyphen_char);
	n_hyphens++;}
      cpos = scan-string;
      c = u8_sgetc(&scan);}
    u8_free(hyphens);
    return n_hyphens;}
}


DEFC_PRIM("hyphenout",hyphenout_prim,
	  KNO_MAX_ARGS(2)|KNO_MIN_ARGS(1),
	  "**undocumented**",
	  {"string",kno_string_type,KNO_VOID},
	  "hyphen_char",kno_character_type,(KNO_CHAR2CODE(0xAD))) /* '­' */
static lispval hyphenout_prim(lispval string,lispval hyphen_char)
{
  U8_OUTPUT *output = u8_current_output;
  u8_string s = KNO_CSTRING(string);
  int len = KNO_STRLEN(string);
  int hc = (KNO_CHARACTERP(hyphen_char)) ? (KNO_CHAR2CODE(hyphen_char)) : (0xAD); /* '­' */
  const u8_byte *scan = s;
  struct U8_OUTPUT word;
  int c = u8_sgetc(&scan);
  if (len==0) return KNO_VOID;
  while ((c>=0)&&(!(u8_isalnum(c)))) {
    u8_putc(output,c);
    c = u8_sgetc(&scan);}
  U8_INIT_OUTPUT(&word,64);
  while (c>=0) {
    if (u8_isalnum(c)) {
      u8_putc(&word,c);
      c = u8_sgetc(&scan);}
    else if (u8_isspace(c)) {
      if (word.u8_write!=word.u8_outbuf) {
	hyphenout_helper
	  (output,word.u8_outbuf,word.u8_write-word.u8_outbuf,hc);
	word.u8_write = word.u8_outbuf;
	word.u8_write[0]='\0';}
      while ((c>=0)&&(!(u8_isalnum(c)))) {
	u8_putc(output,c);
	c = u8_sgetc(&scan);}}
    else {
      int nc = u8_sgetc(&scan);
      if ((nc<0)||(!(u8_isalnum(nc)))) {
	if (word.u8_write!=word.u8_outbuf) {
	  hyphenout_helper
	    (output,word.u8_outbuf,word.u8_write-word.u8_outbuf,hc);
	  word.u8_write = word.u8_outbuf;
	  word.u8_write[0]='\0';}
	u8_putc(output,c);
	if (nc<0) break; else c = nc;
	while ((c>=0)&&(!(u8_isalnum(c)))) {
	  u8_putc(output,c);
	  c = u8_sgetc(&scan);}}
      else {
	u8_putc(&word,c);
	u8_putc(&word,nc);
	c = u8_sgetc(&scan);}}}
  if (word.u8_write!=word.u8_outbuf) {
    hyphenout_helper
      (output,word.u8_outbuf,word.u8_write-word.u8_outbuf,hc);}
  u8_free(word.u8_outbuf);
  u8_flush(output);
  return KNO_VOID;
}


DEFC_PRIM("hyphenate",hyphenate_prim,
	  KNO_MAX_ARGS(2)|KNO_MIN_ARGS(1),
	  "**undocumented**",
	  {"string",kno_string_type,KNO_VOID},
	  {"hyphen_char",kno_character_type,(KNO_CHAR2CODE(0xAD))})  /* '­' */
static lispval hyphenate_prim(lispval string,lispval hyphen_char)
{
  lispval result;
  struct U8_OUTPUT out; U8_OUTPUT *output = &out;
  u8_string s = KNO_CSTRING(string);
  int len = KNO_STRLEN(string);
  int hc = (KNO_CHARACTERP(hyphen_char)) ? (KNO_CHAR2CODE(hyphen_char)) : (0xAD);  /* '­' */
  const u8_byte *scan = s;
  struct U8_OUTPUT word;
  int c = u8_sgetc(&scan);
  if (len==0) return kno_incref(string);
  U8_INIT_OUTPUT(&out,len*2);
  U8_INIT_OUTPUT(&word,64);
  while ((c>=0)&&(!(u8_isalnum(c)))) {
    u8_putc(output,c);
    c = u8_sgetc(&scan);}
  while (c>=0) {
    if (u8_isalnum(c)) {
      u8_putc(&word,c);
      c = u8_sgetc(&scan);}
    else if (u8_isspace(c)) {
      if (word.u8_write!=word.u8_outbuf) {
	hyphenout_helper
	  (output,word.u8_outbuf,word.u8_write-word.u8_outbuf,hc);
	word.u8_write = word.u8_outbuf;
	word.u8_write[0]='\0';}
      while ((c>=0)&&(!(u8_isalnum(c)))) {
	u8_putc(output,c);
	c = u8_sgetc(&scan);}
      if (c<0) break;}
    else {
      int nc = u8_sgetc(&scan);
      if ((nc<0)||(!(u8_isalnum(nc)))) {
	if (word.u8_write!=word.u8_outbuf) {
	  hyphenout_helper
	    (output,word.u8_outbuf,word.u8_write-word.u8_outbuf,hc);
	  word.u8_write = word.u8_outbuf;
	  word.u8_write[0]='\0';}
	u8_putc(output,c);
	if (nc<0) break; else c = nc;
	while ((c>=0)&&(!(u8_isalnum(c)))) {
	  u8_putc(output,c);
	  c = u8_sgetc(&scan);}}
      else {
	u8_putc(&word,c);
	u8_putc(&word,nc);
	c = u8_sgetc(&scan);}}}
  if (word.u8_write!=word.u8_outbuf) {
    hyphenout_helper
      (output,word.u8_outbuf,word.u8_write-word.u8_outbuf,hc);}
  u8_free(word.u8_outbuf);
  result = kno_make_string(NULL,out.u8_write-out.u8_outbuf,out.u8_outbuf);
  u8_free(out.u8_outbuf);
  return result;
}

static lispval hyphenate_module;

KNO_EXPORT int kno_init_hyphenate()
{
  if (hyphenate_init) return 0;

  hyphenate_init = u8_millitime();

  kno_register_config("HYPHENDICT","Where the hyphenate module gets its data",
		      kno_sconfig_get,kno_sconfig_set,&default_hyphenation_file);

  if (default_hyphenation_file)
    default_dict = hnj_hyphen_load(default_hyphenation_file);
  else {
    u8_string dictfile = u8_mkpath(KNO_DATA_DIR,"hyph_en_US.dic");
    if (u8_file_existsp(dictfile))
      default_dict = hnj_hyphen_load(dictfile);
    else {
      u8_log(LOG_CRIT,kno_FileNotFound,
	     "Hyphenation dictionary %s does not exist!",
	     dictfile);}
    u8_free(dictfile);}

  hyphenate_module = kno_new_cmodule("hyphenate",0,kno_init_hyphenate);

  link_local_cprims();

  kno_finish_module(hyphenate_module);

  u8_register_source_file(_FILEINFO);

  return 1;
}

static void link_local_cprims()
{
  u8_string s = "­";
  unsigned int c = u8_sgetc(&s);
  KNO_LINK_CPRIM("shyphenate",shyphenate_prim,1,hyphenate_module);
  KNO_LINK_CPRIM("hyphen-breaks",hyphen_breaks_prim,1,hyphenate_module);
  KNO_LINK_CPRIM("hyphenate-word",hyphenate_word_prim,1,hyphenate_module);

  KNO_LINK_CPRIM("hyphenate",hyphenate_prim,2,hyphenate_module);
  KNO_LINK_CPRIM("hyphenout",hyphenout_prim,2,hyphenate_module);
}
