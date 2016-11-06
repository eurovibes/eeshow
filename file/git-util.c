/*
 * file/git-util.c - Git utility functions
 *
 * Written 2016 by Werner Almesberger
 * Copyright 2016 by Werner Almesberger
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <stddef.h>
#include <stdbool.h>
#include <assert.h>

#include <git2.h>

#include "misc/util.h"
#include "file/git-util.h"


/*
 * This seems to be an efficient way for finding out if a repo is dirty.
 *
 * http://ben.straub.cc/2013/04/02/libgit2-checkout/
 *
 * References:
 * https://libgit2.github.com/libgit2/#HEAD/group/checkout/git_checkout_index
 * https://libgit2.github.com/libgit2/#HEAD/type/git_checkout_options
 * https://github.com/libgit2/libgit2/blob/HEAD/include/git2/checkout.h#L251-295
 */


static int checkout_notify_cb(git_checkout_notify_t why,
    const char *path, const git_diff_file *baseline,
    const git_diff_file *target, const git_diff_file *workdir,
    void *payload)
{
	bool *res = payload;

	assert(why == GIT_CHECKOUT_NOTIFY_DIRTY);

	*res = 1;
	return 0;
}


bool git_repo_is_dirty(git_repository *repo)
{
	git_checkout_options opts;
	bool res = 0;

	/*
	 * Initialization with GIT_CHECKOUT_OPTIONS_INIT complains about not
	 * setting checkout_strategy. git_checkout_init_options is fine.
	 */
	git_checkout_init_options(&opts, GIT_CHECKOUT_OPTIONS_VERSION);
	opts.checkout_strategy = GIT_CHECKOUT_NONE;
		/* let's be explicit about this */
	opts.notify_flags = GIT_CHECKOUT_NOTIFY_DIRTY;
	opts.notify_cb = checkout_notify_cb;
	opts.notify_payload = &res;
	git_checkout_index(repo, NULL, &opts);

	return res;
}


int git_repository_open_ext_caching(git_repository **out, const char *path,
    unsigned int flags, const char *ceiling_dirs)
{
#if LIBGIT2_VER_MAJOR == 0 && LIBGIT2_VER_MINOR < 22
	static struct repo_cache {
		const char *path;
		git_repository *repo;
		struct repo_cache *next;
	} *repo_cache = NULL;
	struct repo_cache *c;
	int res;

	assert(flags == GIT_REPOSITORY_OPEN_CROSS_FS);
	assert(ceiling_dirs == NULL);
	for (c = repo_cache; c; c = c->next)
		if (!strcmp(c->path, path)) {
			*out = c->repo;
			return 0;
		}
	res = git_repository_open_ext(out, path, flags, ceiling_dirs);
	if (res)
		return res;
	c = alloc_type(struct repo_cache);
	c->path = stralloc(path);
	c->repo = *out;
	c->next = repo_cache;
	repo_cache = c;
	return 0;
#else
	return git_repository_open_ext(out, path, flags, ceiling_dirs);
#endif
}

/*
 * Git documentation says that git_libgit2_init can be called more then once
 * but doesn't quite what happens then, e.g., whether references obtained
 * before an init (except for the first, of course) can still be used after
 * it. So we play it safe and initialize only once.
 */

void git_init_once(void)
{
	static bool initialized = 0;

	if (!initialized) {
#if LIBGIT2_VER_MAJOR == 0 && LIBGIT2_VER_MINOR < 22
		git_threads_init();
#else
		git_libgit2_init();
#endif
		initialized = 1;
	}
}
