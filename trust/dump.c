/*
 * Copyright (c) 2013, Red Hat Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above
 *       copyright notice, this list of conditions and the
 *       following disclaimer.
 *     * Redistributions in binary form must reproduce the
 *       above copyright notice, this list of conditions and
 *       the following disclaimer in the documentation and/or
 *       other materials provided with the distribution.
 *     * The names of contributors to this software may not be
 *       used to endorse or promote products derived from this
 *       software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * Author: Stef Walter <stefw@redhat.com>
 */

#include "config.h"

#define P11_DEBUG_FLAG P11_DEBUG_TOOL

#include "attrs.h"
#include "debug.h"
#include "dump.h"
#include "enumerate.h"
#include "message.h"
#include "persist.h"
#include "tool.h"
#include "url.h"

#include "p11-kit/iter.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static char *
format_uri (p11_enumerate *ex,
            int flags)
{
	CK_ATTRIBUTE *attr;
	p11_kit_uri *uri;
	char *string;

	uri = p11_kit_uri_new ();

	memcpy (p11_kit_uri_get_token_info (uri),
	        p11_kit_iter_get_token (ex->iter),
	        sizeof (CK_TOKEN_INFO));

	attr = p11_attrs_find (ex->attrs, CKA_CLASS);
	if (attr != NULL)
		p11_kit_uri_set_attribute (uri, attr);
	attr = p11_attrs_find (ex->attrs, CKA_ID);
	if (attr != NULL)
		p11_kit_uri_set_attribute (uri, attr);

	if (p11_kit_uri_format (uri, flags, &string) != P11_KIT_URI_OK)
		string = NULL;

	p11_kit_uri_free (uri);
	return string;
}

static bool
dump_iterate (p11_enumerate *ex)
{
	p11_persist *persist;
	char *string;
	p11_buffer buf;
	CK_RV rv;

	persist = p11_persist_new ();

	if (!p11_buffer_init (&buf, 0))
		return_val_if_reached (false);

	while ((rv = p11_kit_iter_next (ex->iter)) == CKR_OK) {
		if (!p11_buffer_reset (&buf, 8192))
		         return_val_if_reached (false);

		string = format_uri (ex, P11_KIT_URI_FOR_OBJECT);
		if (string) {
			printf ("# %s\n", string);
			free (string);
		}

		if (!p11_persist_write (persist, ex->attrs, &buf)) {
			p11_message ("could not dump object");
			continue;
		}

		fwrite (buf.data, 1, buf.len, stdout);
		printf ("\n");
	}

	p11_persist_free (persist);
	p11_buffer_uninit (&buf);

	return (rv == CKR_CANCEL);
}

int
p11_trust_dump (int argc,
                char **argv)
{
	p11_enumerate ex;
	int opt = 0;
	int ret;

	enum {
		opt_verbose = 'v',
		opt_quiet = 'q',
		opt_help = 'h',
		opt_filter = 1000,
	};

	struct option options[] = {
		{ "filter", required_argument, NULL, opt_filter },
		{ "verbose", no_argument, NULL, opt_verbose },
		{ "quiet", no_argument, NULL, opt_quiet },
		{ "help", no_argument, NULL, opt_help },
		{ 0 },
	};

	p11_tool_desc usages[] = {
		{ 0, "usage: trust list --filter=<what>" },
		{ opt_filter,
		  "filter of what to export\n"
		  "  pkcs11:object=xx  a PKCS#11 URI\n"
		  "  all               all objects",
		  "what",
		},
		{ opt_verbose, "show verbose debug output", },
		{ opt_quiet, "suppress command output", },
		{ 0 },
	};

	p11_enumerate_init (&ex);

	while ((opt = p11_tool_getopt (argc, argv, options)) != -1) {
		switch (opt) {
		case opt_verbose:
		case opt_quiet:
			break;

		case opt_filter:
			if (!p11_enumerate_opt_filter (&ex, optarg))
				exit (2);
			break;
		case 'h':
			p11_tool_usage (usages, options);
			exit (0);
		case '?':
			exit (2);
		default:
			assert_not_reached ();
			break;
		}
	}

	if (argc - optind != 0) {
		p11_message ("extra arguments passed to command");
		exit (2);
	}

	if (!p11_enumerate_ready (&ex, "all"))
		exit (1);

	ret = dump_iterate (&ex) ? 0 : 1;

	p11_enumerate_cleanup (&ex);
	return ret;
}
