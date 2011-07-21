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
 * svr_modify.c
 *
 * Functions relating to the Modify Job Batch Requests.
 *
 * Included funtions are:
 *
 *
 */
#include <pbs_config.h>   /* the master config generated by configure */

#include <stdio.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>
#include <pthread.h>
#include "libpbs.h"
#include "server_limits.h"
#include "list_link.h"
#include "attribute.h"
#include "resource.h"
#include "server.h"
#include "queue.h"
#include "credential.h"
#include "batch_request.h"
#include "pbs_job.h"
#include "work_task.h"
#include "pbs_error.h"
#include "log.h"
#include "svrfunc.h"
#include "array.h"

#define CHK_HOLD 1
#define CHK_CONT 2


/* Global Data Items: */

extern attribute_def     job_attr_def[];
extern char *msg_jobmod;
extern char *msg_manager;
extern char *msg_mombadmodify;
extern int   comp_resc_lt;
extern int   LOGLEVEL;
extern char *path_checkpoint;
extern char server_name[];
extern time_t time_now;

extern const char *PJobSubState[];
extern char *PJobState[];

/* External Functions called */

extern void cleanup_restart_file(job *);
extern void rel_resc(job *);

extern job  *chk_job_request(char *, struct batch_request *);
extern struct batch_request *cpy_checkpoint(struct batch_request *, job *, enum job_atr, int);

/* prototypes */
void post_modify_arrayreq(struct work_task *pwt);
static void post_modify_req(struct work_task *pwt);


/*
 * post_modify_req - clean up after sending modify request to MOM
 */

static void post_modify_req(

  struct work_task *pwt)

  {

  struct batch_request *preq;
  job  *pjob;
  char                  log_buf[LOCAL_LOG_BUF_SIZE];

  svr_disconnect(pwt->wt_event);  /* close connection to MOM */

  preq = pwt->wt_parm1;

  preq->rq_conn = preq->rq_orgconn;  /* restore socket to client */

  if ((preq->rq_reply.brp_code) && (preq->rq_reply.brp_code != PBSE_UNKJOBID))
    {
    sprintf(log_buf, msg_mombadmodify, preq->rq_reply.brp_code);

    log_event(
      PBSEVENT_JOB,
      PBS_EVENTCLASS_JOB,
      preq->rq_ind.rq_modify.rq_objname,
      log_buf);

    req_reject(preq->rq_reply.brp_code, 0, preq, NULL, NULL);
    }
  else
    {
    if (preq->rq_reply.brp_code == PBSE_UNKJOBID)
      {
      if ((pjob = find_job(preq->rq_ind.rq_modify.rq_objname)) == NULL)
        {
        req_reject(preq->rq_reply.brp_code, 0, preq, NULL, NULL);
        free(pwt);
        return;
        }
      else
        {
        if (LOGLEVEL >= 0)
          {
          sprintf(log_buf, "post_modify_req: PBSE_UNKJOBID for job %s in state %s-%s, dest = %s",
            (pjob->ji_qs.ji_jobid != NULL) ? pjob->ji_qs.ji_jobid : "",
            PJobState[pjob->ji_qs.ji_state],
            PJobSubState[pjob->ji_qs.ji_substate],
            pjob->ji_qs.ji_destin);

          log_event(PBSEVENT_JOB,PBS_EVENTCLASS_JOB,pjob->ji_qs.ji_jobid,log_buf);
          }
        
        pthread_mutex_unlock(pjob->ji_mutex);
        }
      }

    reply_ack(preq);
    }

  free(pwt);

  return;
  }  /* END post_modify_req() */



/*
 * mom_cleanup_checkpoint_hold - Handle the clean up of mom after checkpoint and
 * hold.  This gets messy because there is a race condition between getting the
 * job obit and having the copy checkpoint complete.  After both have occured
 * we can request the mom to cleanup the job
 */

void mom_cleanup_checkpoint_hold(

  struct work_task *ptask)

  {
  static char          *id = "mom_cleanup_checkpoint_hold";

  int                   rc = 0;
  job                  *pjob;
  char                 *jobid;

  struct batch_request *preq;
  char                  log_buf[LOCAL_LOG_BUF_SIZE];

  jobid = (char *)ptask->wt_parm1;
  free(ptask);

  if (jobid == NULL)
    {
    log_err(ENOMEM,id,"Cannot allocate memory");
    return;
    }

  pjob = find_job(jobid);
  free(jobid);

  if (LOGLEVEL >= 7)
    {
    sprintf(log_buf,
      "checking mom cleanup job state is %s-%s\n",
      PJobState[pjob->ji_qs.ji_state],
      PJobSubState[pjob->ji_qs.ji_substate]);

    log_event(PBSEVENT_JOB,PBS_EVENTCLASS_JOB,pjob->ji_qs.ji_jobid,log_buf);
    }

  /* 
   * if the job is no longer running then we have recieved the job obit
   * and need to request the mom to clean up after the job
   */

  if (pjob->ji_qs.ji_state != JOB_STATE_RUNNING)
    {
    if ((preq = alloc_br(PBS_BATCH_DeleteJob)) == NULL)
      {
      log_err(-1, id, "unable to allocate DeleteJob request - big trouble!");
      }
    else
      {
      strcpy(preq->rq_ind.rq_delete.rq_objname, pjob->ji_qs.ji_jobid);

      if ((rc = relay_to_mom(pjob, preq, release_req)) != 0)
        {
        snprintf(log_buf,sizeof(log_buf),
          "Unable to relay information to mom for job '%s'\n",
          pjob->ji_qs.ji_jobid);

        log_err(rc,id,log_buf);
        free_br(preq);

        pthread_mutex_unlock(pjob->ji_mutex);

        return;
        }

      if (LOGLEVEL >= 7)
        {
        LOG_EVENT(
          PBSEVENT_JOB,
          PBS_EVENTCLASS_JOB,
          pjob->ji_qs.ji_jobid,
          "requested mom cleanup");
        }
      }
    }
  else
    {
    set_task(WORK_Timed, time_now + 1, mom_cleanup_checkpoint_hold, strdup(pjob->ji_qs.ji_jobid), FALSE);
    }

  pthread_mutex_unlock(pjob->ji_mutex);
  } /* END mom_cleanup_checkpoint_hold() */




/*
 * chkpt_xfr_hold - Handle the clean up of the transfer of the checkpoint files.
 */

void chkpt_xfr_hold(

  struct work_task *ptask)

  {
  job       *pjob;

  struct batch_request *preq;
  char                  log_buf[LOCAL_LOG_BUF_SIZE];

  preq = (struct batch_request *)ptask->wt_parm1;
  pjob = (job *)preq->rq_extra;

  pthread_mutex_lock(pjob->ji_mutex);

  if (LOGLEVEL >= 7)
    {
    sprintf(log_buf,
      "BLCR copy completed (state is %s-%s)",
      PJobState[pjob->ji_qs.ji_state],
      PJobSubState[pjob->ji_qs.ji_substate]);

    log_event(PBSEVENT_JOB,PBS_EVENTCLASS_JOB,pjob->ji_qs.ji_jobid,log_buf);
    }
  
  release_req(ptask);

  set_task(WORK_Immed, 0, mom_cleanup_checkpoint_hold, strdup(pjob->ji_qs.ji_jobid), FALSE);

  pthread_mutex_unlock(pjob->ji_mutex);

  return;
  }  /* END chkpt_xfr_hold() */





/*
 * chkpt_xfr_done - Handle the clean up of the transfer of the checkpoint files.
 */

void chkpt_xfr_done(

  struct work_task *ptask)

  {
  job       *pjob;

  struct batch_request *preq;

  preq = (struct batch_request *)ptask->wt_parm1;
  pjob = (job *)preq->rq_extra;

  /* Why are we grabbing a pointer to the job or the request here??? 
   * Nothing is done??!!?? 
   * If implemented later, thread protection must be added */
  
  release_req(ptask);

  return;
  }  /* END chkpt_xfr_done() */





/*
 * modify_job()
 * modifies a job according to the newattr
 *
 * @param j - the job being altered
 * @param newattr - the new attributes
 * @return SUCCESS if set, FAILURE if problems
 */

int modify_job(

  void      *j,               /* O */
  svrattrl  *plist,           /* I */
  struct batch_request *preq, /* I */
  int        checkpoint_req,  /* I */
  int        flag)            /* I */

  {
  int   bad = 0;
  int   i;
  int   newstate;
  int   newsubstate;
  resource_def *prsd;
  int   rc;
  int   sendmom = 0;
  int   copy_checkpoint_files = FALSE;

  char *id = "modify_job";
  char  log_buf[LOCAL_LOG_BUF_SIZE];

  job *pjob = (job *)j;
  
  /* cannot be in exiting or transit, exiting has already been checked */

  if (pjob->ji_qs.ji_state == JOB_STATE_TRANSIT)
    {
    /* FAILURE */
    snprintf(log_buf,sizeof(log_buf),
      "Cannot modify job '%s' in transit\n",
      pjob->ji_qs.ji_jobid);

    log_err(PBSE_BADSTATE,id,log_buf);

    return(PBSE_BADSTATE);
    }

  if (((checkpoint_req == CHK_HOLD) || (checkpoint_req == CHK_CONT)) &&
      (pjob->ji_qs.ji_substate == JOB_SUBSTATE_RUNNING))
    {
    /* May need to request copy of the checkpoint file from mom */

    copy_checkpoint_files = TRUE;

    if (checkpoint_req == CHK_HOLD)
      {

      sprintf(log_buf,"setting jobsubstate for %s to RERUN\n", pjob->ji_qs.ji_jobid);

      pjob->ji_qs.ji_substate = JOB_SUBSTATE_RERUN;

      job_save(pjob, SAVEJOB_QUICK, 0);

      LOG_EVENT(PBSEVENT_JOB, PBS_EVENTCLASS_JOB, pjob->ji_qs.ji_jobid, log_buf);

      /* remove checkpoint restart file if there is one */
      
      if (pjob->ji_wattr[JOB_ATR_restart_name].at_flags & ATR_VFLAG_SET)
        {
        cleanup_restart_file(pjob);
        }

      }
    }

  /* if job is running, special checks must be made */

  /* NOTE:  must determine if job exists down at MOM - this will occur if
            job is running, job is held, or job was held and just barely
            released (ie qhold/qrls) */

  /* COMMENTED OUT BY JOSH B IN 2.3 DUE TO MAJOR PROBLEMS w/ CUSTOMERS
   * --FIX and uncomment once we know what is really going on.
   *
   * We now know that ji_destin gets set on a qmove and that the mom does not
   * have the job at that point.
   *
  if ((pjob->ji_qs.ji_state == JOB_STATE_RUNNING) ||
     ((pjob->ji_qs.ji_state == JOB_STATE_HELD) && (pjob->ji_qs.ji_destin[0] != '\0')) ||
     ((pjob->ji_qs.ji_state == JOB_STATE_QUEUED) && (pjob->ji_qs.ji_destin[0] != '\0')))
  */
  if (pjob->ji_qs.ji_state == JOB_STATE_RUNNING)
    {
    while (plist != NULL)
      {
      /* is the attribute modifiable in RUN state ? */

      i = find_attr(job_attr_def, plist->al_name, JOB_ATR_LAST);

      if ((i < 0) ||
          ((job_attr_def[i].at_flags & ATR_DFLAG_ALTRUN) == 0))
        {
        /* FAILURE */
        snprintf(log_buf,sizeof(log_buf),
          "Cannot modify attribute '%s' while running\n",
          plist->al_name);
        log_err(PBSE_MODATRRUN,id,log_buf);

        return PBSE_MODATRRUN;
        }

      /* NOTE:  only explicitly specified job attributes are routed down to MOM */

      if (i == JOB_ATR_resource)
        {
        /* is the specified resource modifiable while */
        /* the job is running                         */

        prsd = find_resc_def(
                 svr_resc_def,
                 plist->al_resc,
                 svr_resc_size);

        if (prsd == NULL)
          {
          /* FAILURE */
          snprintf(log_buf,sizeof(log_buf),
            "Unknown attribute '%s'\n",
            plist->al_name);

          log_err(PBSE_UNKRESC,id,log_buf);

          return(PBSE_UNKRESC);
          }

        if ((prsd->rs_flags & ATR_DFLAG_ALTRUN) == 0)
          {
          /* FAILURE */
          snprintf(log_buf,sizeof(log_buf),
            "Cannot modify attribute '%s' while running\n",
            plist->al_name);
          log_err(PBSE_MODATRRUN,id,log_buf);

          return(PBSE_MODATRRUN);
          }

        sendmom = 1;
        }
/*
        else if ((i == JOB_ATR_checkpoint_name) || (i == JOB_ATR_variables))
        {
        sendmom = 1;
        }
*/

      plist = (svrattrl *)GET_NEXT(plist->al_link);
      }
    }    /* END if (pjob->ji_qs.ji_state == JOB_STATE_RUNNING) */

  /* modify the job's attributes */

  bad = 0;

  plist = (svrattrl *)GET_NEXT(preq->rq_ind.rq_modify.rq_attr);

  rc = modify_job_attr(pjob, plist, preq->rq_perm, &bad);

  if (rc)
    {
    /* FAILURE */
    snprintf(log_buf,sizeof(log_buf),
      "Cannot set attributes for job '%s'\n",
      pjob->ji_qs.ji_jobid);
    log_err(rc,id,log_buf);

    return(rc);
    }

  /* Reset any defaults resource limit which might have been unset */

  set_resc_deflt(pjob, NULL);

  /* if job is not running, may need to change its state */

  if (pjob->ji_qs.ji_state != JOB_STATE_RUNNING)
    {
    svr_evaljobstate(pjob, &newstate, &newsubstate, 0);

    svr_setjobstate(pjob, newstate, newsubstate);
    }
  else
    {
    job_save(pjob, SAVEJOB_FULL, 0);
    }

  sprintf(log_buf, msg_manager, msg_jobmod, preq->rq_user, preq->rq_host);

  log_event(PBSEVENT_JOB,PBS_EVENTCLASS_JOB,pjob->ji_qs.ji_jobid,log_buf);

  /* if a resource limit changed for a running job, send to MOM */

  if (sendmom)
    {
    /* if the NO_MOM_RELAY flag is set the calling function will call
       relay_to_mom so we do not need to do it here */
    if (flag != NO_MOM_RELAY)
      {
      if ((rc = relay_to_mom(
                  pjob,
                  preq,
                  post_modify_req)))
        {  
        snprintf(log_buf,sizeof(log_buf),
          "Unable to relay information to mom for job '%s'\n",
          pjob->ji_qs.ji_jobid);

        log_err(rc,id,log_buf);
  
        return(rc); /* unable to get to MOM */
        }
      }

    return(PBSE_RELAYED_TO_MOM);
    }

  if (copy_checkpoint_files)
    {
    struct batch_request *momreq = 0;
    momreq = cpy_checkpoint(momreq, pjob, JOB_ATR_checkpoint_name, CKPT_DIR_OUT);

    if (momreq != NULL)
      {
      /* have files to copy */

      momreq->rq_extra = (void *)pjob;

      if (checkpoint_req == CHK_HOLD)
        {
        rc = relay_to_mom(pjob, momreq, chkpt_xfr_hold);
        }
      else
        {
        rc = relay_to_mom(pjob, momreq, chkpt_xfr_done);
        }

      if (rc != 0)
        {
        snprintf(log_buf,sizeof(log_buf),
          "Unable to relay information to mom for job '%s'\n",
          pjob->ji_qs.ji_jobid);

        log_err(rc,id,log_buf);

        return(0);  /* come back when mom replies */
        }
      }
    else
      {
      log_err(-1,id, "Failed to get batch request");
      }
    }

  return(0);
  } /* END modify_job() */


int copy_batchrequest(
    
  struct batch_request **newreq,
  struct batch_request *preq,
  int type,
  int jobid)

  {
  struct batch_request *request;
  svrattrl *pal = NULL;
  svrattrl *newpal = NULL;
  tlist_head *phead = NULL;
  char *ptr1, *ptr2;
  char newjobname[PBS_MAXSVRJOBID+1];
  
  request = alloc_br(type);
  if (request)
    {
    request->rq_type = preq->rq_type;
    request->rq_perm = preq->rq_perm;
    request->rq_fromsvr = preq->rq_fromsvr;
    request->rq_conn = preq->rq_conn;
    request->rq_orgconn = preq->rq_orgconn;
    request->rq_extsz = preq->rq_extsz;
    request->rq_time = preq->rq_time;
    strcpy(request->rq_user, preq->rq_user);
    strcpy(request->rq_host, preq->rq_host);
    request->rq_reply.brp_choice = preq->rq_reply.brp_choice;
    request->rq_noreply = preq->rq_noreply;
    /* we need to copy rq_extend if there is any data */
    if (preq->rq_extend)
      {
      request->rq_extend = (char *)malloc(strlen(preq->rq_extend) + 1);
      if (request->rq_extend == NULL)
        {
        return(PBSE_SYSTEM);
        }
      strcpy(request->rq_extend, preq->rq_extend);
      }
    /* remember the batch_request we copied */
    request->rq_extra = (void *)preq;
    
    switch(preq->rq_type)
      {
      /* This function was created for a modify arracy request (PBS_BATCH_ModifyJob)
         the preq->rq_ind structure was allocated in dis_request_read. If other
         BATCH types are needed refer to that function to see how the rq_ind structure
         was allocated and then copy it here. */
      case PBS_BATCH_DeleteJob:
        
      case PBS_BATCH_HoldJob:
        
      case PBS_BATCH_CheckpointJob:
        
      case PBS_BATCH_ModifyJob:
        
      case PBS_BATCH_AsyModifyJob:
        /* based on how decode_DIS_Manage allocates data */
        CLEAR_HEAD(request->rq_ind.rq_manager.rq_attr);
        
        phead = &request->rq_ind.rq_manager.rq_attr;
        request->rq_ind.rq_manager.rq_cmd = preq->rq_ind.rq_manager.rq_cmd;
        request->rq_ind.rq_manager.rq_objtype = preq->rq_ind.rq_manager.rq_objtype;
        /* If this is a job array it is possible we only have the array name
           and not the individual job. We need to find out what we have and
           modify the name if needed */
        ptr1 = strstr(preq->rq_ind.rq_manager.rq_objname, "[]");
        if (ptr1)
          {
          ptr1++;
          strcpy(newjobname, preq->rq_ind.rq_manager.rq_objname);
          ptr2 = strstr(newjobname, "[]");
          ptr2++;
          *ptr2 = 0;
          sprintf(request->rq_ind.rq_manager.rq_objname,"%s%d%s", 
            newjobname,
            jobid,
            ptr1);
          }
        else
          strcpy(request->rq_ind.rq_manager.rq_objname, preq->rq_ind.rq_manager.rq_objname);
        
        /* copy the attribute list */
        pal = (svrattrl *)GET_NEXT(preq->rq_ind.rq_manager.rq_attr);
        while(pal != NULL)
          {
          newpal = (svrattrl *)malloc(pal->al_tsize + 1);
          if (!newpal)
            {
            return(PBSE_SYSTEM);
            }
          CLEAR_LINK(newpal->al_link);
          
          newpal->al_atopl.next = 0;
          newpal->al_tsize = pal->al_tsize + 1;
          newpal->al_nameln = pal->al_nameln;
          newpal->al_flags  = pal->al_flags;
          newpal->al_atopl.name = (char *)newpal + sizeof(svrattrl);
          strcpy(newpal->al_atopl.name, pal->al_atopl.name);
          newpal->al_nameln = pal->al_nameln;
          newpal->al_atopl.resource = newpal->al_atopl.name + newpal->al_nameln;
          strcpy(newpal->al_atopl.resource, pal->al_atopl.resource);
          newpal->al_rescln = pal->al_rescln;
          newpal->al_atopl.value = newpal->al_atopl.name + newpal->al_nameln + newpal->al_rescln;
          strcpy(newpal->al_atopl.value, pal->al_atopl.value);
          newpal->al_valln = pal->al_valln;
          newpal->al_atopl.op = pal->al_atopl.op;
          
          pal = (struct svrattrl *)GET_NEXT(pal->al_link);
          
          }
        
        break;
        
      default:
        break;
        
      }
    request->rq_ind.rq_manager.rq_cmd = preq->rq_ind.rq_manager.rq_cmd;
    request->rq_ind.rq_manager.rq_objtype = preq->rq_ind.rq_manager.rq_objtype;
    append_link(phead, &newpal->al_link, newpal);
    
    *newreq = request;
    return(0);
    
    }
  else
    return(PBSE_SYSTEM);
  }


/*
 * modify_whole_array()
 * modifies the entire job array 
 * @SEE req_modify_array PARENT
 */ 
int modify_whole_array(

  job_array *pa,              /* I/O */
  svrattrl  *plist,           /* I */
  struct batch_request *preq, /* I */
  int        checkpoint_req)  /* I */

  {
  char id[] = "modify_whole_array";
  int  i;
  int  rc = 0;
  int  mom_relay = 0;
  char log_buf[LOCAL_LOG_BUF_SIZE];

  for (i = 0; i < pa->ai_qs.array_size; i++)
    {
    if (pa->jobs[i] == NULL)
      continue;

    pthread_mutex_lock(pa->jobs[i]->ji_mutex);

    /* NO_MOM_RELAY will prevent modify_job from calling relay_to_mom */
    rc = modify_job(pa->jobs[i],plist,preq,checkpoint_req, NO_MOM_RELAY);

    if (rc == PBSE_RELAYED_TO_MOM)
      {
      struct batch_request *array_req = NULL;

      /* We told modify_job not to call relay_to_mom so we need to contact the mom */
      rc = copy_batchrequest(&array_req, preq, 0, i);
      if (rc != 0)
        {
        pthread_mutex_unlock(pa->jobs[i]->ji_mutex);
        
        return(rc);
        }

      preq->rq_refcount++;
      if (mom_relay == 0)
        {
        preq->rq_refcount++;
        }
      mom_relay++;
      if ((rc = relay_to_mom(
                  pa->jobs[i],
                  array_req,
                  post_modify_arrayreq)))
        {  
        snprintf(log_buf,sizeof(log_buf),
          "Unable to relay information to mom for job '%s'\n",
          pa->jobs[i]->ji_qs.ji_jobid);

        log_err(rc,id,log_buf);

        pthread_mutex_unlock(pa->jobs[i]->ji_mutex);
        
        return(rc); /* unable to get to MOM */
        }

      }

    pthread_mutex_unlock(pa->jobs[i]->ji_mutex);
    }

  if (mom_relay)
    {
    preq->rq_refcount--;
    if (preq->rq_refcount == 0)
      {
      free_br(preq);
      }
    return(PBSE_RELAYED_TO_MOM);
    }

  return(rc);
  } /* END modify_whole_array() */



/*
 * req_modifyarray()
 * modifies a job array
 * additionally, can change the slot limit of the array
 */

void *req_modifyarray(

  void *vp) /* I */

  {
  job_array            *pa;
  job                  *pjob;
  svrattrl             *plist;
  int                   checkpoint_req = FALSE;
  char                 *array_spec = NULL;
  char                 *pcnt = NULL;
  int                   rc = 0;
  int                   rc2 = 0;
  struct batch_request *preq = (struct batch_request *)vp;

  pa = get_array(preq->rq_ind.rq_modify.rq_objname);

  if (pa == NULL)
    {
    req_reject(PBSE_IVALREQ,0,preq,NULL,"Cannot find array");

    return(NULL);
    }

  plist = (svrattrl *)GET_NEXT(preq->rq_ind.rq_modify.rq_attr);

  /* If async modify, reply now; otherwise reply is handled later */
  if (preq->rq_type == PBS_BATCH_AsyModifyJob)
    {
    reply_ack(preq);

    preq->rq_noreply = TRUE; /* set for no more replies */
    }

  /* pbs_mom sets the extend string to trigger copying of checkpoint files */

  if (preq->rq_extend != NULL)
    {
    if (strcmp(preq->rq_extend,CHECKPOINTHOLD) == 0)
      {
      checkpoint_req = CHK_HOLD;
      }
    else if (strcmp(preq->rq_extend,CHECKPOINTCONT) == 0)
      {
      checkpoint_req = CHK_CONT;
      }
    }

  /* find if an array range was specified */
  if ((preq->rq_extend != NULL) && 
      ((array_spec = strstr(preq->rq_extend,ARRAY_RANGE)) != NULL))
    {
    /* move array spec past ARRAY_RANGE= */
    char *equals = strchr(array_spec,'=');
    if (equals != NULL)
      {
      array_spec = equals + 1;
      }

    if ((pcnt = strchr(array_spec,'%')) != NULL)
      {
      int slot_limit = atoi(pcnt+1);
      pa->ai_qs.slot_limit = slot_limit;
      }
    }

  if ((array_spec != NULL) &&
      (pcnt != array_spec))
    {
    if (pcnt != NULL)
      *pcnt = '\0';

    /* there is more than just a slot given, modify that range */
    rc = modify_array_range(pa,array_spec,plist,preq,checkpoint_req);

    if ((rc != 0) && 
       (rc != PBSE_RELAYED_TO_MOM))
      {
      pthread_mutex_unlock(pa->ai_mutex);

      req_reject(PBSE_IVALREQ,0,preq,NULL,"Error reading array range");
  
      return(NULL);
      }

    if (pcnt != NULL)
      *pcnt = '%';

    if (rc == PBSE_RELAYED_TO_MOM)
      {
      pthread_mutex_unlock(pa->ai_mutex);

      return(NULL);
      }
    }
  else 
    {
    rc = modify_whole_array(pa,plist,preq,checkpoint_req);

    if ((rc != 0) && 
        (rc != PBSE_RELAYED_TO_MOM))
      {
      pthread_mutex_unlock(pa->ai_mutex);

      req_reject(PBSE_IVALREQ,0,preq,NULL,"Error altering the array");
      return(NULL);
      }

    /* we modified the job array. We now need to update the job */
    pjob = chk_job_request(preq->rq_ind.rq_modify.rq_objname, preq);
    rc2 = modify_job(pjob,plist,preq,checkpoint_req, NO_MOM_RELAY);

    if ((rc2) && 
        (rc != PBSE_RELAYED_TO_MOM))
      {
      /* there are two operations going on that give a return code:
         one from modify_whole_array and one from modify_job_for_array.
         If either of these fail, return the error. This makes it
         so some elements fo the array will be updated but others are
         not. But at least the user will know something went wrong.*/
      pthread_mutex_unlock(pa->ai_mutex);
      pthread_mutex_unlock(pjob->ji_mutex);

      req_reject(rc,0,preq,NULL,NULL);
      return(NULL);
      }

    if (rc == PBSE_RELAYED_TO_MOM)
      {
      pthread_mutex_unlock(pa->ai_mutex);
      pthread_mutex_unlock(pjob->ji_mutex);
      
      return(NULL);
      }

    pthread_mutex_unlock(pjob->ji_mutex);
    }

  /* SUCCESS */
  pthread_mutex_unlock(pa->ai_mutex);

  reply_ack(preq);

  return(NULL);
  } /* END req_modifyarray() */





/*
 * req_modifyjob - service the Modify Job Request
 *
 * This request modifes a job's attributes.
 *
 * @see relay_to_mom() - child - routes change down to pbs_mom
 */

void *req_modifyjob(

  void *vp) /* I */

  {
  job  *pjob;
  svrattrl *plist;
  int   rc;
  int   checkpoint_req = FALSE;
  struct batch_request *preq = (struct batch_request *)vp;

  pjob = chk_job_request(preq->rq_ind.rq_modify.rq_objname, preq);

  if (pjob == NULL)
    {
    return(NULL);
    }

  plist = (svrattrl *)GET_NEXT(preq->rq_ind.rq_modify.rq_attr);

  if (plist == NULL)
    {
    /* nothing to do */

    reply_ack(preq);

    /* SUCCESS */
    pthread_mutex_unlock(pjob->ji_mutex);

    return(NULL);
    }

  /* If async modify, reply now; otherwise reply is handled later */
  if (preq->rq_type == PBS_BATCH_AsyModifyJob)
    {
    reply_ack(preq);

    preq->rq_noreply = TRUE; /* set for no more replies */
    }

  /* pbs_mom sets the extend string to trigger copying of checkpoint files */

  if (preq->rq_extend != NULL)
    {
    if (strcmp(preq->rq_extend,CHECKPOINTHOLD) == 0)
      {
      checkpoint_req = CHK_HOLD;
      }
    else if (strcmp(preq->rq_extend,CHECKPOINTCONT) == 0)
      {
      checkpoint_req = CHK_CONT;
      }
    }

  if ((rc = modify_job(pjob,plist,preq,checkpoint_req, 0)) != 0)
    {
    if ((rc == PBSE_MODATRRUN) ||
        (rc == PBSE_UNKRESC))
      reply_badattr(rc,1,plist,preq);
    else if ( rc == PBSE_RELAYED_TO_MOM )
      {
      pthread_mutex_unlock(pjob->ji_mutex);
      
      return(NULL);
      }
    else
      req_reject(rc,0,preq,NULL,NULL);
    }
  else
    reply_ack(preq);

  pthread_mutex_unlock(pjob->ji_mutex);
  
  return(NULL);
  }  /* END req_modifyjob() */





/*
 * modify_job_attr - modify the attributes of a job atomically
 * Used by req_modifyjob() to alter the job attributes and by
 * stat_update() [see req_stat.c] to update with latest from MOM
 */

int modify_job_attr(

  job    *pjob,  /* I (modified) */
  svrattrl *plist, /* I */
  int     perm,
  int    *bad)   /* O */

  {
  int        allow_unkn;
  long       i;
  attribute  newattr[JOB_ATR_LAST];
  attribute *pattr;
  int        rc;
  char       log_buf[LOCAL_LOG_BUF_SIZE];

  if (pjob->ji_qhdr->qu_qs.qu_type == QTYPE_Execution)
    allow_unkn = -1;
  else
    allow_unkn = JOB_ATR_UNKN;

  pattr = pjob->ji_wattr;

  /* call attr_atomic_set to decode and set a copy of the attributes */

  rc = attr_atomic_set(
         plist,        /* I */
         pattr,        /* I */
         newattr,      /* O */
         job_attr_def, /* I */
         JOB_ATR_LAST,
         allow_unkn,   /* I */
         perm,         /* I */
         bad);         /* O */

  /* if resource limits are being changed ... */

  if ((rc == 0) &&
      (newattr[JOB_ATR_resource].at_flags & ATR_VFLAG_SET))
    {
    if ((perm & (ATR_DFLAG_MGWR | ATR_DFLAG_OPWR)) == 0)
      {
      /* If job is running, only manager/operator can raise limits */

      if (pjob->ji_qs.ji_state == JOB_STATE_RUNNING)
        {
        int comp_resc_lt = comp_resc2(&pjob->ji_wattr[JOB_ATR_resource],
                                      &newattr[JOB_ATR_resource],
                                      server.sv_attr[SRV_ATR_QCQLimits].at_val.at_long,
                                      NULL,
                                      LESS);

        if (comp_resc_lt != 0)
          {
          rc = PBSE_PERM;
          }
        }

      /* Also check against queue and system limits */

      if (rc == 0)
        {
        rc = chk_resc_limits(
               &newattr[JOB_ATR_resource],
               pjob->ji_qhdr,
               NULL);
        }
      }
    }    /* END if ((rc == 0) && ...) */

  /* special check on permissions for hold */

  if ((rc == 0) &&
      (newattr[JOB_ATR_hold].at_flags & ATR_VFLAG_MODIFY))
    {
    i = newattr[JOB_ATR_hold].at_val.at_long ^
        (pattr + JOB_ATR_hold)->at_val.at_long;

    rc = chk_hold_priv(i, perm);
    }

  if (rc == 0)
    {
    for (i = 0;i < JOB_ATR_LAST;i++)
      {
      if (newattr[i].at_flags & ATR_VFLAG_MODIFY)
        {
        if (job_attr_def[i].at_action)
          {
          rc = job_attr_def[i].at_action(
                 &newattr[i],
                 pjob,
                 ATR_ACTION_ALTER);

          if (rc)
            break;
          }
        }
      }    /* END for (i) */

    if ((rc == 0) &&
        ((newattr[JOB_ATR_userlst].at_flags & ATR_VFLAG_MODIFY) ||
         (newattr[JOB_ATR_grouplst].at_flags & ATR_VFLAG_MODIFY)))
      {
      /* need to reset execution uid and gid */

      rc = set_jobexid(pjob, newattr, NULL);
      }

    if ((rc == 0) &&
        (newattr[JOB_ATR_outpath].at_flags & ATR_VFLAG_MODIFY))
      {
      /* need to recheck if JOB_ATR_outpath is a special case of host only */

      if (newattr[JOB_ATR_outpath].at_val.at_str[strlen(newattr[JOB_ATR_outpath].at_val.at_str) - 1] == ':')
        {
        newattr[JOB_ATR_outpath].at_val.at_str =
          prefix_std_file(pjob, (int)'o');
        }
      /*
       * if the output path was specified and ends with a '/'
       * then append the standard file name
       */
      else if (newattr[JOB_ATR_outpath].at_val.at_str[strlen(newattr[JOB_ATR_outpath].at_val.at_str) - 1] == '/')
        {
          newattr[JOB_ATR_outpath].at_val.at_str[strlen(newattr[JOB_ATR_outpath].at_val.at_str) - 1] = '\0';
          
          replace_attr_string(&newattr[JOB_ATR_outpath],
                            (add_std_filename(pjob,
                            newattr[JOB_ATR_outpath].at_val.at_str,
                            (int)'o')));
        }
      }

    if ((rc == 0) &&
        (newattr[JOB_ATR_errpath].at_flags & ATR_VFLAG_MODIFY))
      {
      /* need to recheck if JOB_ATR_errpath is a special case of host only */

      if (newattr[JOB_ATR_errpath].at_val.at_str[strlen(newattr[JOB_ATR_errpath].at_val.at_str) - 1] == ':')
        {
        newattr[JOB_ATR_errpath].at_val.at_str =
          prefix_std_file(pjob, (int)'e');
        }
      /*
       * if the error path was specified and ends with a '/'
       * then append the standard file name
       */
      else if (newattr[JOB_ATR_errpath].at_val.at_str[strlen(newattr[JOB_ATR_errpath].at_val.at_str) - 1] == '/')
        {
          newattr[JOB_ATR_errpath].at_val.at_str[strlen(newattr[JOB_ATR_errpath].at_val.at_str) - 1] = '\0';
          
          replace_attr_string(&newattr[JOB_ATR_errpath],
                            (add_std_filename(pjob,
                            newattr[JOB_ATR_errpath].at_val.at_str,
                            (int)'e')));
        }
      }

    }  /* END if (rc == 0) */

  if (rc != 0)
    {
    for (i = 0;i < JOB_ATR_LAST;i++)
      job_attr_def[i].at_free(newattr + i);

    /* FAILURE */

    return(rc);
    }  /* END if (rc != 0) */

  /* OK, now copy the new values into the job attribute array */

  for (i = 0;i < JOB_ATR_LAST;i++)
    {
    if (newattr[i].at_flags & ATR_VFLAG_MODIFY)
      {
      if (LOGLEVEL >= 7)
        {
        sprintf(log_buf, "attr %s modified", job_attr_def[i].at_name);

        log_event(PBSEVENT_JOB,PBS_EVENTCLASS_JOB,pjob->ji_qs.ji_jobid,log_buf);
        }

      job_attr_def[i].at_free(pattr + i);

      if ((newattr[i].at_type == ATR_TYPE_LIST) ||
          (newattr[i].at_type == ATR_TYPE_RESC))
        {
        list_move(
          &newattr[i].at_val.at_list,
          &(pattr + i)->at_val.at_list);
        }
      else
        {
        *(pattr + i) = newattr[i];
        }

      (pattr + i)->at_flags = newattr[i].at_flags;
      }
    }    /* END for (i) */

  /* note, the newattr[] attributes are on the stack, they go away automatically */

  pjob->ji_modified = 1;

  return(0);
  }  /* END modify_job_attr() */

/*
 * post_modify_arrayreq - clean up after sending modify request to MOM
 */

void post_modify_arrayreq(

  struct work_task *pwt)

  {

  struct batch_request *preq;
  struct batch_request *parent_req;
  job                  *pjob;
  char                  log_buf[LOCAL_LOG_BUF_SIZE];

  svr_disconnect(pwt->wt_event);  /* close connection to MOM */

  preq = pwt->wt_parm1;
  parent_req = preq->rq_extra; /* This is the original batch_request allocated by process_request */

  preq->rq_conn = preq->rq_orgconn;  /* restore socket to client */

  if ((preq->rq_reply.brp_code) && (preq->rq_reply.brp_code != PBSE_UNKJOBID))
    {
    sprintf(log_buf, msg_mombadmodify, preq->rq_reply.brp_code);

    log_event(PBSEVENT_JOB,PBS_EVENTCLASS_JOB,preq->rq_ind.rq_modify.rq_objname,log_buf);

    parent_req->rq_refcount--;
    if (parent_req->rq_refcount == 0)
      {
      free_br(preq);
      req_reject(parent_req->rq_reply.brp_code, 0, parent_req, NULL, NULL);
      }
    else
      free_br(preq);
    }
  else
    {
    if (preq->rq_reply.brp_code == PBSE_UNKJOBID)
      {
      if ((pjob = find_job(preq->rq_ind.rq_modify.rq_objname)) == NULL)
        {
        parent_req->rq_refcount--;

        free(pwt);

        if (parent_req->rq_refcount == 0)
          {
          free_br(preq);
          req_reject(parent_req->rq_reply.brp_code, 0, parent_req, NULL, NULL);
          return;
          }
        else
          {
          free_br(preq);
          return;
          }
        }
      else
        {
        if (LOGLEVEL >= 0)
          {
          sprintf(log_buf, "post_modify_req: PBSE_UNKJOBID for job %s in state %s-%s, dest = %s",
            (pjob->ji_qs.ji_jobid != NULL) ? pjob->ji_qs.ji_jobid : "",
            PJobState[pjob->ji_qs.ji_state],
            PJobSubState[pjob->ji_qs.ji_substate],
            pjob->ji_qs.ji_destin);

          log_event(PBSEVENT_JOB,PBS_EVENTCLASS_JOB,pjob->ji_qs.ji_jobid,log_buf);
          }
        
        pthread_mutex_unlock(pjob->ji_mutex);
        }
      }

    parent_req->rq_refcount--;
    if (parent_req->rq_refcount == 0)
      {
      parent_req->rq_reply.brp_code = preq->rq_reply.brp_code;
      free_br(preq);
      reply_ack(parent_req);
      }
    else
      free_br(preq);
    }

  free(pwt);

  return;
  }  /* END post_modify_arrayreq() */

