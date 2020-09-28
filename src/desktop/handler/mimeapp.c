/* $Id$ */
/* Copyright (c) 2020 Pierre Pronchery <khorben@defora.org> */
/* This file is part of DeforaOS Desktop Browser */
/* Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY ITS AUTHORS AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */



#include "mimeapp.h"


/* MimeApp */
/* private */
/* types */
struct _MimeApp
{
	MimeHandler * mime;
	String * datadir;
};


/* public */
/* functions */
/* mimeapp_new */
MimeApp * mimeapp_new(MimeHandler * mime, String const * datadir)
{
	MimeApp * mimeapp;

	if((mimeapp = object_new(sizeof(*mimeapp))) == NULL)
		return NULL;
	mimeapp->mime = NULL;
	if(datadir == NULL)
		mimeapp->datadir = NULL;
	else if((mimeapp->datadir = string_new(datadir)) == NULL)
	{
		mimeapp_delete(mimeapp);
		return NULL;
	}
	mimeapp->mime = mime;
	return mimeapp;
}


/* mimeapp_delete */
void mimeapp_delete(MimeApp * mimeapp)
{
	if(mimeapp->mime != NULL)
		mimehandler_delete(mimeapp->mime);
	string_delete(mimeapp->datadir);
	object_delete(mimeapp);
}


/* accessors */
/* mimeapp_get_mime */
MimeHandler * mimeapp_get_mime(MimeApp const * mimeapp)
{
	return mimeapp->mime;
}


/* mimeapp_get_datadir */
String const * mimeapp_get_datadir(MimeApp const * mimeapp)
{
	return mimeapp->datadir;
}


/* mimeapp_compare */
int mimeapp_compare(MimeApp const * mime, MimeApp const * to)
{
	MimeHandler * mha;
	MimeHandler * mhb;
	String const * mhas;
	String const * mhbs;

	mha = mimeapp_get_mime(mime);
	mhb = mimeapp_get_mime(to);
	if((mhas = mimehandler_get_generic_name(mha, 1)) == NULL)
		mhas = mimehandler_get_name(mha, 1);
	if((mhbs = mimehandler_get_generic_name(mhb, 1)) == NULL)
		mhbs = mimehandler_get_name(mhb, 1);
	return string_compare(mhas, mhbs);
}
