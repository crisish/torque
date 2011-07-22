/*
*         OpenPBS (Portable Batch System) v2.3 Software License
*
* Copyright (c) 1999-2000 Veridian Information Solutions, Inc.
* All rights reserved.
*
* ---------------------------------------------------------------------------
* For a license to use or redistribute the OpenPBS software under conditions
* other than those described below, or to purchase support for this software,
* please contact Veridian Systems, PBS Products Department ("Licensor") at:
*
*    www.OpenPBS.org  +1 650 967-4675                  sales@OpenPBS.org
*                        877 902-4PBS (US toll-free)
* ---------------------------------------------------------------------------
*
* This license covers use of the OpenPBS v2.3 software (the "Software") at
* your site or location, and, for certain users, redistribution of the
* Software to other sites and locations.  Use and redistribution of
* OpenPBS v2.3 in source and binary forms, with or without modification,
* are permitted provided that all of the following conditions are met.
* After December 31, 2001, only conditions 3-6 must be met:
*
* 1. Commercial and/or non-commercial use of the Software is permitted
*    provided a current software registration is on file at www.OpenPBS.org.
*    If use of this software contributes to a publication, product, or
*    service, proper attribution must be given; see www.OpenPBS.org/credit.html
*
* 2. Redistribution in any form is only permitted for non-commercial,
*    non-profit purposes.  There can be no charge for the Software or any
*    software incorporating the Software.  Further, there can be no
*    expectation of revenue generated as a consequence of redistributing
*    the Software.
*
* 3. Any Redistribution of source code must retain the above copyright notice
*    and the acknowledgment contained in paragraph 6, this list of conditions
*    and the disclaimer contained in paragraph 7.
*
* 4. Any Redistribution in binary form must reproduce the above copyright
*    notice and the acknowledgment contained in paragraph 6, this list of
*    conditions and the disclaimer contained in paragraph 7 in the
*    documentation and/or other materials provided with the distribution.
*
* 5. Redistributions in any form must be accompanied by information on how to
*    obtain complete source code for the OpenPBS software and any
*    modifications and/or additions to the OpenPBS software.  The source code
*    must either be included in the distribution or be available for no more
*    than the cost of distribution plus a nominal fee, and all modifications
*    and additions to the Software must be freely redistributable by any party
*    (including Licensor) without restriction.
*
* 6. All advertising materials mentioning features or use of the Software must
*    display the following acknowledgment:
*
*     "This product includes software developed by NASA Ames Research Center,
*     Lawrence Livermore National Laboratory, and Veridian Information
*     Solutions, Inc.
*     Visit www.OpenPBS.org for OpenPBS software support,
*     products, and information."
*
* 7. DISCLAIMER OF WARRANTY
*
* THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND. ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT
* ARE EXPRESSLY DISCLAIMED.
*
* IN NO EVENT SHALL VERIDIAN CORPORATION, ITS AFFILIATED COMPANIES, OR THE
* U.S. GOVERNMENT OR ANY OF ITS AGENCIES BE LIABLE FOR ANY DIRECT OR INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
* This license will be governed by the laws of the Commonwealth of Virginia,
* without reference to its choice of law rules.
*/




/*
 * add_verify_resources
 *
 * Append a entries to the attribute list that are from the resource list.
 * If the add flag is set, append the resource regardless. Otherwise, append
 * it only if it is not already on the list.
 *
 */

#include <pbs_config.h>   /* the master config generated by configure */

#define TRUE 1
#define FALSE 0

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "pbs_ifl.h"
#include "pbs_cmds.h"
#include "u_hash_map_structs.h"
#include "u_memmgr.h"

int add_verify_resources(

  memmgr        **mm,       /* M */
  job_data      **res_attr, /* M */
  char          *resources, /* I */
  int            p_type)    /* I */

  {
  char *r, *eq, *v, *e = NULL;
  char *str;
  char *name;
  char *value = NULL;

  int len;

  char *qptr = NULL;

  r = resources;

  while (*r != '\0')
    {
    /* skip leading whitespace */

    while (isspace((int)*r))
      r++;

    /* get resource name */

    eq = r;

    while ((*eq != '=') && (*eq != ',') && (*eq != '\0'))
      eq++;

    /* make sure there is a resource name */

    if (r == eq)
      {
      /* FAILURE */

      return(1);
      }

    /*
     * Count the number of non-space characters that make up the
     * resource name.  Count only up to the last character before the
     * separator ('\0', ',' or '=').
     */

    for (str = r, len = 0;(str < eq) && !isspace((int)*str);str++)
      len++;

    /* if separated by an equal sign, get the value */

    if (*eq == '=')
      {
      char *ptr;

      v = eq + 1;

      while (isspace((int)*v))
        v++;

      /* FORMAT: <ATTR>=[{'"}]<VAL>,<VAL>[{'"}][,<ATTR>=...]... */

      ptr = strchr(v, ',');

      if (((qptr = strchr(v, '\'')) != NULL) && (ptr != NULL) && (qptr < ptr))
        {
        v = qptr + 1;
        }
      else if (((qptr = strchr(v, '\"')) != NULL) && (ptr != NULL) && (qptr < ptr))
        {
        v = qptr + 1;
        }
      else
        {
        qptr = NULL;
        }

      e = v;

      while (*e != '\0')
        {
        /* FORMAT: <ATTR>=[{'"}]<VAL>,<VAL>[{'"}][,<ATTR>=...]... */

        /* NOTE    already tokenized by getopt() which will support
                   quoted whitespace, do not fail on spaces */

        if (qptr != NULL)
          {
          /* value contains quote - only terminate with quote */

          if ((*e == '\'') || (*e == '\"'))
            break;
          }
        else
          {
          if (*e == ',')
            break;
          }

#ifdef TNOT
        if (isspace((int)*e))
          {
          /* FAILURE */

          return(1);
          }

#endif /* TNOT */

        e++;
        }  /* END while (*e != '\0') */
      }    /* END if (*eq == '=') */
    else
      {
      v = NULL;
      }

    /* This code in combination with the backend server code ends up with
     * the logic of last added element remains. All others are dropped off.
     * Instead of posponing all of this logic, it will now occur here.
     */
    name = (char *)memmgr_calloc(mm, 1, len + 1);
    if (v)
      value = (char *)memmgr_calloc(mm, 1, (e - v) + 1);
    if ((name) && ((v) && (value)))
      {
      strncpy(name, r, len);
      if (v)
        {
        strncpy(value, v, e - v);
        hash_add_or_exit(mm, res_attr, name, value, p_type);
        }
      else
        hash_add_or_exit(mm, res_attr, name, "\0", p_type);
      }
    else
      {
      fprintf(stderr, "Error allocating memory for add_verify_resources");
      exit(1);
      }
    /* Allocate memory for the attrl structure */

/*    attr = (struct attrl *)malloc(sizeof(struct attrl));

    if (attr == NULL)
      {
      fprintf(stderr, "Out of memory\n");

      FAILURE

      exit(2);
      }
      */

    /* Allocate memory for the attribute name and copy */

/*    str = (char *)malloc(strlen(ATTR_l) + 1);

    if (str == NULL)
      {
      fprintf(stderr, "Out of memory\n");

      exit(2);
      }

    strcpy(str, ATTR_l);

    attr->name = str;
    */

    /* Allocate memory for the resource name and copy */

/*    str = (char *)malloc(len + 1);

    if (str == NULL)
      {
      fprintf(stderr, "Out of memory\n");

      exit(2);
      }

    strncpy(str, r, len);

    str[len] = '\0';

    attr->resource = str;
    */

    /* Allocate memory for the value and copy */

/*    if (v != NULL)
      {
      str = (char *)malloc(e - v + 1);

      if (str == NULL)
        {
        fprintf(stderr, "Out of memory\n");

        exit(2);
        }

      strncpy(str, v, e - v);

      str[e - v] = '\0';

      attr->value = str;
      }
    else
      {
      str = (char *)malloc(1);

      if (str == NULL)
        {
        fprintf(stderr, "Out of memory\n");

        exit(2);
        }

      str[0] = '\0';

      attr->value = str;
      }
      */

    /* Put it on the attribute list */

    /* If the argument add is true, add to the list regardless.
     * Otherwise, add it to the list only if the resource name
     * is not already on the list.
     */

    /*
    attr->next = NULL;

    if (*attrib == NULL)
      {
      *attrib = attr;
      }
    else
      {
      ap = *attrib;

      found = FALSE;

      while (ap->next != NULL)
        {
        if (!strcmp(ap->name, ATTR_l) && !strcmp(ap->resource, attr->resource))
          found = TRUE;

        ap = ap->next;
        }

      if (add || !found)
        ap->next = attr;
      }
      */

    /* Get ready for next resource/value pair */

    if (qptr != NULL)
      {
      /* skip quotes looking for ',' */

      while ((*e == '\'') || (*e == '\"'))
        e++;
      }

    if (v != NULL)
      r = e;
    else
      r = eq;

    if (*r == ',')
      {
      r++;

      if (*r == '\0')
        {
        return(1);
        }
      }
    }      /* END while (*r != '\0') */

  /* SUCCESS */

  return(0);
  }  /* END add_verify_resources() */


