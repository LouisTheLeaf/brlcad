/*                         S L A V E . C
 * BRL-CAD / ADRT
 *
 * Copyright (c) 2007-2022 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file slave.c
 *
 */

#include "common.h"

/* interface header */
#include "./slave.h"

/* system headers */
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
#endif
#ifdef HAVE_GETOPT_H
#  include <getopt.h>
#endif
#include "bio.h"

/* public api headers */
#include "bu/app.h"
#include "bu/getopt.h"
#include "bu/str.h"
#include "rt/tie.h"

/* adrt headers */
#include "adrt.h"
#include "load.h"
#include "camera.h"
#include "render_util.h"
#include "tienet_slave.h"


typedef struct adrt_slave_project_s {
    struct tie_s tie;
    render_camera_t camera;
    uint16_t last_frame;
    uint8_t active;
} adrt_slave_project_t;

uint32_t adrt_slave_threads;
adrt_slave_project_t adrt_workspace_list[ADRT_MAX_WORKSPACE_NUM];

void
adrt_slave_free()
{
    uint16_t i;

    for (i = 0; i < ADRT_MAX_WORKSPACE_NUM; i++)
	if (adrt_workspace_list[i].active) {
/*      render_camera_free (&camera); */
	}
}

void
adrt_slave_work(tienet_buffer_t *work, tienet_buffer_t *result)
{
    TIE_3 pos, foc;
    unsigned char rm, op;
    uint32_t ind = 0;
    uint16_t wid = 0;

    /* Get work type */
    TCOPY(uint8_t, work->data, ind, &op, 0);
    ind += 1;

    /* Workspace ID */
    TCOPY(uint16_t, work->data, ind, &wid, 0);
    ind += 2;

    /* This will get improved later with caching */
    TIENET_BUFFER_SIZE((*result), 3); /* Copy op and wid, 3 bytes */
    memcpy(result->data, &work->data[0], 3);
    result->ind = ind;

    switch (op) {
	case ADRT_WORK_INIT:
	{
	    render_camera_init (&adrt_workspace_list[wid].camera, adrt_slave_threads);
	    if ( slave_load (&adrt_workspace_list[wid].tie, (void *)work->data) != 0 )
		bu_exit (1, "Failed to load geometry. Going into a flaming tailspin\n");
	    TIE_PREP(&adrt_workspace_list[wid].tie);
	    render_camera_prep (&adrt_workspace_list[wid].camera);
	    printf ("ready.\n");
	    result->ind = 0;

	    /* Mark the workspace as active so it can be cleaned up when the time comes. */
	    adrt_workspace_list[wid].active = 1;
	}
	break;

	case ADRT_WORK_STATUS:
	{
#ifndef HAVE_DECL_GETLOADAVG
	    extern int getloadavg(double loadavg[], int nelem);
#endif /* HAVE_DECL_GETLOADAVG */
#ifdef HAVE_GETLOADAVG
	    double loadavg = -1.0;
	    getloadavg (&loadavg, 1);
	    printf ("load average: %f\n", loadavg);
#endif
	}
	break;

	case ADRT_WORK_SELECT:
	{
	    uint8_t c;
	    char string[255];
	    uint32_t n, i, num;

	    ind = 1;	/* ind is too far in for some reason, force it back to 1? */

	    /* reset */
	    TCOPY(uint8_t, work->data, ind, &c, 0);
	    ind += 1;
	    if (c)
		for (i = 0; i < slave_load_mesh_num; i++)
		    slave_load_mesh_list[i].flags = 0;

	    /* number of strings to match */
	    TCOPY(uint32_t, work->data, ind, &num, 0);
	    ind += 4;

	    for (i = 0; i < num; i++) {
		/* string length */
		TCOPY(uint8_t, work->data, ind, &c, 0);	/* this likes to break, 'num' is way too big. */
		ind += 1;

		/* string */
		memcpy(string, &work->data[ind], c);
		ind += c;

		/* set select flag */
		for (n = 0; n < slave_load_mesh_num; n++)
		    if (strstr(slave_load_mesh_list[n].name, string) || c == 1)
			slave_load_mesh_list[n].flags = (slave_load_mesh_list[n].flags & 0x1) | ((slave_load_mesh_list[n].flags & 0x2) ^ 0x2);
	    }

	    /* zero length result */
	    result->ind = 0;
	}
	break;

	case ADRT_WORK_SHOTLINE:
	{
	    struct tie_ray_s ray;
	    void *mesg;
	    int dlen;

	    mesg = NULL;

	    /* coordinates */
	    TCOPY(TIE_3, work->data, ind, ray.pos, 0);
	    ind += sizeof (TIE_3);
	    TCOPY(TIE_3, work->data, ind, ray.dir, 0);

	    /* Fire the shot */
	    ray.depth = 0;
	    render_util_shotline_list (&adrt_workspace_list[wid].tie, &ray, &mesg, &dlen);

	    /* Make room for shot data */
	    TIENET_BUFFER_SIZE((*result), result->ind + dlen + 2*sizeof (TIE_3));
	    memcpy(&result->data[result->ind], mesg, dlen);
	    result->ind += dlen;

	    TCOPY(TIE_3, &ray.pos, 0, result->data, result->ind);
	    result->ind += sizeof (TIE_3);

	    TCOPY(TIE_3, &ray.dir, 0, result->data, result->ind);
	    result->ind += sizeof (TIE_3);

	    free (mesg);
	}
	break;

	case ADRT_WORK_SPALL:
	{
#if 0
	    tie_ray_t ray;
	    TFLOAT angle;
	    void *mesg;
	    int dlen;

	    mesg = NULL;

	    /* position */
	    TCOPY(TIE_3, data, ind, &ray.pos, 0);
	    ind += sizeof (TIE_3);

	    /* direction */
	    TCOPY(TIE_3, data, ind, &ray.dir, 0);
	    ind += sizeof (TIE_3);

	    /* angle */
	    TCOPY(TFLOAT, data, ind, &angle, 0);
	    ind += sizeof (TFLOAT);

	    /* Fire the shot */
	    ray.depth = 0;
	    render_util_spall_list(tie, &ray, angle, &mesg, &dlen);

	    /* Make room for shot data */
	    *res_len = sizeof (common_work_t) + dlen;
	    *res_buf = (void *)realloc(*res_buf, *res_len);

	    ind = 0;

	    memcpy(&((char *)*res_buf)[ind], mesg, dlen);

	    free (mesg);
#endif
	}
	break;

	case ADRT_WORK_FRAME_ATTR:
	{
	    uint16_t image_w, image_h, image_format;

	    /* Image Size */
	    TCOPY(uint16_t, work->data, ind, &image_w, 0);
	    ind += 2;
	    TCOPY(uint16_t, work->data, ind, &image_h, 0);
	    ind += 2;
	    TCOPY(uint16_t, work->data, ind, &image_format, 0);

	    adrt_workspace_list[wid].camera.w = image_w;
	    adrt_workspace_list[wid].camera.h = image_h;
	    render_camera_prep (&adrt_workspace_list[wid].camera);
	    result->ind = 0;
	}
	break;

	case ADRT_WORK_FRAME:
	{
	    camera_tile_t tile;
	    uint8_t type;
	    TFLOAT fov;

	    /* Camera type */
	    TCOPY(uint8_t, work->data, ind, &type, 0);
	    ind += 1;

	    /* Camera fov */
	    TCOPY(TFLOAT, work->data, ind, &fov, 0);
	    ind += sizeof (TFLOAT);

	    /* Camera position */
	    TCOPY(TIE_3, work->data, ind, &pos, 0);
	    ind += sizeof (TIE_3);

	    /* Camera Focus */
	    TCOPY(TIE_3, work->data, ind, &foc, 0);
	    ind += sizeof (TIE_3);

	    /* Update Rendering Method if it has Changed */
	    rm = work->data[ind];
	    ind += 1;

	    if (rm != adrt_workspace_list[wid].camera.rm || ADRT_MESSAGE_MODE_CHANGEP(rm)) {
		rm = ADRT_MESSAGE_MODE(rm);

		adrt_workspace_list[wid].camera.render.free (&adrt_workspace_list[wid].camera.render);

		switch (rm) {
		    case RENDER_METHOD_DEPTH:
			render_depth_init(&adrt_workspace_list[wid].camera.render, NULL);
			break;

		    case RENDER_METHOD_COMPONENT:
			render_component_init(&adrt_workspace_list[wid].camera.render, NULL);
			break;

		    case RENDER_METHOD_FLOS:
		    {
			TIE_3 frag_pos;
			char buf[BUFSIZ];

			/* Extract shot position and direction */
			TCOPY(TIE_3, work->data, ind, &frag_pos, 0);
			snprintf(buf, BUFSIZ, "#(%f %f %f)", V3ARGS(frag_pos.v));
			render_flos_init(&adrt_workspace_list[wid].camera.render, buf);
		    }
		    break;

		    case RENDER_METHOD_GRID:
			render_grid_init(&adrt_workspace_list[wid].camera.render, NULL);
			break;

		    case RENDER_METHOD_NORMAL:
			render_normal_init(&adrt_workspace_list[wid].camera.render, NULL);
			break;

		    case RENDER_METHOD_PATH:
			render_path_init(&adrt_workspace_list[wid].camera.render, "12");
			break;

		    case RENDER_METHOD_PHONG:
			render_phong_init(&adrt_workspace_list[wid].camera.render, NULL);
			break;

		    case RENDER_METHOD_CUT:
			render_cut_init(&adrt_workspace_list[wid].camera.render, (char *)work->data + ind);
			break;

		    case RENDER_METHOD_SPALL:
		    {
			TIE_3 shot_pos, shot_dir;
			TFLOAT angle;
			char buf[BUFSIZ];

			/* Extract shot position and direction */
			TCOPY(TIE_3, work->data, ind, &shot_pos, 0);
			ind += sizeof (TIE_3);

			TCOPY(TIE_3, work->data, ind, &shot_dir, 0);
			ind += sizeof (TIE_3);

			TCOPY(TFLOAT, work->data, ind, &angle, 0);

			snprintf(buf, BUFSIZ, "(%g %g %g) (%g %g %g) %g", V3ARGS(shot_pos.v), V3ARGS(shot_dir.v), angle);

			render_spall_init (&adrt_workspace_list[wid].camera.render, buf);
		    }
		    break;

		    default:
			break;
		}

		adrt_workspace_list[wid].camera.rm = rm;
	    }

	    /* The portion of the image to be rendered */
	    ind = work->ind - sizeof (camera_tile_t);
	    TCOPY(camera_tile_t, work->data, ind, &tile, 0);

	    /* Update camera if different frame */
	    if (tile.frame != adrt_workspace_list[wid].last_frame) {
		adrt_workspace_list[wid].camera.type = type;
		adrt_workspace_list[wid].camera.fov = fov;

		TCOPY(TIE_3, adrt_workspace_list[wid].camera.pos, 0, &pos, 0);
		TCOPY(TIE_3, adrt_workspace_list[wid].camera.focus, 0, &foc, 0);

		render_camera_prep (&adrt_workspace_list[wid].camera);
	    }
	    adrt_workspace_list[wid].last_frame = tile.frame;

	    render_camera_render (&adrt_workspace_list[wid].camera, &adrt_workspace_list[wid].tie, &tile, result);
	}
	break;

	case ADRT_WORK_MINMAX:
	{
	    TCOPY(TIE_3, &adrt_workspace_list[wid].tie.min, 0, result->data, result->ind);
	    result->ind += sizeof (TIE_3);

	    TCOPY(TIE_3, &adrt_workspace_list[wid].tie.max, 0, result->data, result->ind);
	    result->ind += sizeof (TIE_3);
	}
	break;

	default:
	    break;
    }

#if 0
    {
	struct timeval	tv;
	static int      adrt_slave_completed = 0;
	static time_t	adrt_slave_startsec = 0;

	if (adrt_slave_startsec == 0)
	    adrt_slave_startsec = time(NULL);

	gettimeofday(&tv, NULL);
	printf("\t[Work Units Completed: %.6d  Rays: %.5d k/sec %lld]\n",
		++adrt_slave_completed,
		(int) ((TFLOAT) adrt_workspace_list[wid].tie.rays_fired / (TFLOAT) (1000 * (tv.tv_sec - adrt_slave_startsec + 1))),
		adrt_workspace_list[wid].tie.rays_fired);
	fflush(stdout);
    }
#endif

    return;
}

void
adrt_slave(int port, char *host, int threads)
{
    int i;
    adrt_slave_threads = threads;
    tienet_slave_init(port, host, adrt_slave_work, adrt_slave_free, ADRT_VER_KEY);

    /* Initialize all workspaces as inactive */
    for (i = 0; i < ADRT_MAX_WORKSPACE_NUM; i++)
	adrt_workspace_list[i].active = 0;
}


#ifdef HAVE_GETOPT_LONG
static struct option longopts[] =
{
    { "help",	no_argument,		NULL, 'h' },
    { "port",	required_argument,	NULL, 'p' },
    { "threads",	required_argument,	NULL, 't' },
    { "version",	no_argument,		NULL, 'v' },
};
#endif
static char shortopts[] = "Xdhp:t:vh?";


static void finish(int sig)
{
    bu_exit(EXIT_FAILURE, "Collected signal %d, aborting!\n", sig);
}

static void info(int UNUSED(sig))
{
	/* something to display info about clients, threads and port. */
    return;
}

static void help()
{
    fprintf(stderr,"%s\n", ADRT_VER_DETAIL);
    fprintf(stderr,"%s", "Usage: adrt_slave [options] [host]\n\
  -v\t\tdisplay version\n\
  -h\t\tdisplay help\n\
  -p\t\tport number\n\
  -t ...\tnumber of threads to launch for processing\n");
}


int
main(int argc, char **argv)
{
    int		port = 0, c = 0, threads = 0;
    char		host[64], temp[64];

    bu_setprogname(argv[0]);

    /* Default Port */
    signal(SIGINT, finish);
#ifdef SIGUSR1
    signal(SIGUSR1, info);
#endif
#ifdef SIGINFO
    signal(SIGINFO, info);
#endif

    /* Initialize strings */
    host[0] = 0;
    port = 0;


    /* Parse command line options */

    while ((c =
#ifdef HAVE_GETOPT_LONG
	    getopt_long(argc, argv, shortopts, longopts, NULL)
#else
	    bu_getopt(argc, argv, shortopts)
#endif
	       )!= -1) {
	if (bu_optopt == '?') c='h';
	switch (c) {
	    case 'h':
		help();
		return EXIT_SUCCESS;

	    case 'p':
		port = atoi(bu_optarg);
		break;

	    case 't':
		bu_strlcpy(temp, bu_optarg, 5);
		threads = atoi(temp);
		CLAMP(threads, 0, 32);
		break;

	    case 'v':
		printf("%s\n", ADRT_VER_DETAIL);
		return EXIT_SUCCESS;

	    default:
		help();
		return EXIT_FAILURE;
	}
    }

    argc -= bu_optind;
    argv += bu_optind;

    if (argc)
	bu_strlcpy(host, argv[0], 64);

    if (!host[0]) {
	if (!port)
	    port = TN_SLAVE_PORT;
	printf("running as daemon.\n");
    } else {
	if (!port)
	    port = TN_MASTER_PORT;
    }

    adrt_slave(port, host, threads);

    return EXIT_SUCCESS;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
