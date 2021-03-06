/****************************************************************************
 * examples/nxlines/nxlines_main.c
 *
 *   Copyright (C) 2011-2012 Gregory Nutt. All rights reserved.
 *   Author: Gregory Nutt <gnutt@nuttx.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/types.h>

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <sched.h>
#include <errno.h>
#include <debug.h>

#ifdef CONFIG_NX_LCDDRIVER
#  include <nuttx/lcd/lcd.h>
#else
#  include <nuttx/video/fb.h>
#endif

#include <nuttx/arch.h>
#include <nuttx/nx/nx.h>
#include <nuttx/nx/nxglib.h>

#include "nxlines.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Configuration ************************************************************/
/* If not specified, assume that the hardware supports one video plane */

#ifndef CONFIG_EXAMPLES_NXLINES_VPLANE
#  define CONFIG_EXAMPLES_NXLINES_VPLANE 0
#endif

/* If not specified, assume that the hardware supports one LCD device */

#ifndef CONFIG_EXAMPLES_NXLINES_DEVNO
#  define CONFIG_EXAMPLES_NXLINES_DEVNO 0
#endif

/****************************************************************************
 * Private Types
 ****************************************************************************/

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

/****************************************************************************
 * Public Data
 ****************************************************************************/

struct nxlines_data_s g_nxlines =
{
  NULL,          /* hnx */
  NULL,          /* hbkgd */
  0,             /* xres */
  0,             /* yres */
  false,         /* havpos */
  { 0 },         /* sem */
  NXEXIT_SUCCESS /* exit code */
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: nxlines_initialize
 ****************************************************************************/

static inline int nxlines_initialize(void)
{
  FAR NX_DRIVERTYPE *dev;

#if defined(CONFIG_EXAMPLES_NXLINES_EXTERNINIT)
  /* Use external graphics driver initialization */

  message("nxlines_initialize: Initializing external graphics device\n");
  dev = up_nxdrvinit(CONFIG_EXAMPLES_NXLINES_DEVNO);
  if (!dev)
    {
      message("nxlines_initialize: up_nxdrvinit failed, devno=%d\n",
              CONFIG_EXAMPLES_NXLINES_DEVNO);
      g_nxlines.code = NXEXIT_EXTINITIALIZE;
      return ERROR;
    }

#elif defined(CONFIG_NX_LCDDRIVER)
  int ret;

  /* Initialize the LCD device */

  message("nxlines_initialize: Initializing LCD\n");
  ret = up_lcdinitialize();
  if (ret < 0)
    {
      message("nxlines_initialize: up_lcdinitialize failed: %d\n", -ret);
      g_nxlines.code = NXEXIT_LCDINITIALIZE;
      return ERROR;
    }

  /* Get the device instance */

  dev = up_lcdgetdev(CONFIG_EXAMPLES_NXLINES_DEVNO);
  if (!dev)
    {
      message("nxlines_initialize: up_lcdgetdev failed, devno=%d\n", CONFIG_EXAMPLES_NXLINES_DEVNO);
      g_nxlines.code = NXEXIT_LCDGETDEV;
      return ERROR;
    }

  /* Turn the LCD on at 75% power */

  (void)dev->setpower(dev, ((3*CONFIG_LCD_MAXPOWER + 3)/4));
#else
  int ret;

  /* Initialize the frame buffer device */

  message("nxlines_initialize: Initializing framebuffer\n");
  ret = up_fbinitialize();
  if (ret < 0)
    {
      message("nxlines_initialize: up_fbinitialize failed: %d\n", -ret);
      g_nxlines.code = NXEXIT_FBINITIALIZE;
      return ERROR;
    }

  dev = up_fbgetvplane(CONFIG_EXAMPLES_NXLINES_VPLANE);
  if (!dev)
    {
      message("nxlines_initialize: up_fbgetvplane failed, vplane=%d\n", CONFIG_EXAMPLES_NXLINES_VPLANE);
      g_nxlines.code = NXEXIT_FBGETVPLANE;
      return ERROR;
    }
#endif

  /* Then open NX */

  message("nxlines_initialize: Open NX\n");
  g_nxlines.hnx = nx_open(dev);
  if (!g_nxlines.hnx)
    {
      message("nxlines_initialize: nx_open failed: %d\n", errno);
      g_nxlines.code = NXEXIT_NXOPEN;
      return ERROR;
    }

  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: nxlines_main
 ****************************************************************************/

int nxlines_main(int argc, char *argv[])
{
  nxgl_mxpixel_t color;
  int ret;

  /* Initialize NX */

  ret = nxlines_initialize();
  message("nxlines_main: NX handle=%p\n", g_nxlines.hnx);
  if (!g_nxlines.hnx || ret < 0)
    {
      message("nxlines_main: Failed to get NX handle: %d\n", errno);
      g_nxlines.code = NXEXIT_NXOPEN;
      goto errout;
    }

  /* Set the background to the configured background color */

  message("nxlines_main: Set background color=%d\n",
          CONFIG_EXAMPLES_NXLINES_BGCOLOR);

  color = CONFIG_EXAMPLES_NXLINES_BGCOLOR;
  ret = nx_setbgcolor(g_nxlines.hnx, &color);
  if (ret < 0)
    {
      message("nxlines_main: nx_setbgcolor failed: %d\n", errno);
      g_nxlines.code = NXEXIT_NXSETBGCOLOR;
      goto errout_with_nx;
    }

  /* Get the background window */

  ret = nx_requestbkgd(g_nxlines.hnx, &g_nxlinescb, NULL);
  if (ret < 0)
    {
      message("nxlines_main: nx_setbgcolor failed: %d\n", errno);
      g_nxlines.code = NXEXIT_NXREQUESTBKGD;
      goto errout_with_nx;
    }

  /* Wait until we have the screen resolution.  We'll have this immediately
   * unless we are dealing with the NX server.
   */

  while (!g_nxlines.havepos)
    {
      (void)sem_wait(&g_nxlines.sem);
    }
  message("nxlines_main: Screen resolution (%d,%d)\n", g_nxlines.xres, g_nxlines.yres);

  /* Now, say perform the lines (these test does not return so the remaining
   * logic is cosmetic). 
   */

  nxlines_test(g_nxlines.hbkgd);

  /* Release background */

  (void)nx_releasebkgd(g_nxlines.hbkgd);

  /* Close NX */

errout_with_nx:
  message("nxlines_main: Close NX\n");
  nx_close(g_nxlines.hnx);
errout:
  return g_nxlines.code;
}
