#include "license_pbs.h" /* See here for the software license */
#include "lib_ifl.h"
#include "test_PBSD_submit_caps.h"
#include <stdlib.h>
#include <stdio.h>


#include "pbs_error.h"

int PBSD_scbuf(int c, int reqtype, int seq, char *buf, int len, char *jobid, enum job_file which);

START_TEST(test_PBSD_queuejob)
  {
  fail_unless(PBSD_queuejob(-1, NULL, NULL, NULL, NULL, NULL) == NULL);
  fail_unless(PBSD_queuejob(PBS_NET_MAX_CONNECTIONS, NULL, NULL, NULL, NULL, NULL) == NULL);

  }
END_TEST


START_TEST(test_PBSD_scbuf)
  {
  fail_unless(PBSD_scbuf(-1, 0, 0, NULL, 0, NULL, JScript) == PBSE_IVALREQ);
  fail_unless(PBSD_scbuf(PBS_NET_MAX_CONNECTIONS, 0, 0, NULL, 0, NULL, JScript) == PBSE_IVALREQ);
  }
END_TEST


START_TEST(test_PBSD_commit)
  {
  fail_unless(PBSD_commit(-1, NULL) == PBSE_IVALREQ);
  fail_unless(PBSD_commit(PBS_NET_MAX_CONNECTIONS, NULL) == PBSE_IVALREQ);
  }
END_TEST


START_TEST(test_PBSD_commit_get_sid)
  {
  fail_unless(PBSD_commit_get_sid(-1, NULL, NULL) == PBSE_IVALREQ);
  fail_unless(PBSD_commit_get_sid(PBS_NET_MAX_CONNECTIONS, NULL, NULL) == PBSE_IVALREQ);
  }
END_TEST


START_TEST(test_PBSD_rdytocmt)
  {
  fail_unless(PBSD_rdytocmt(-1, NULL) == PBSE_IVALREQ);
  fail_unless(PBSD_rdytocmt(PBS_NET_MAX_CONNECTIONS, NULL) == PBSE_IVALREQ);
  }
END_TEST


START_TEST(test_PBSD_QueueJob_hash)
  {
  char *jobid = NULL;
  char *destin = NULL;
  job_data *job_attr = NULL;
  job_data *res_attr = NULL;
  char *extend = NULL;
  char *msg;
  memmgr *mm;
  fail_unless(PBSD_QueueJob_hash(-1, jobid, destin, &mm, job_attr, res_attr, extend, &jobid, &msg) == PBSE_IVALREQ);
  fail_unless(PBSD_QueueJob_hash(PBS_NET_MAX_CONNECTIONS, jobid, destin, &mm, job_attr, res_attr, extend, &jobid, &msg) == PBSE_IVALREQ);

  }
END_TEST


START_TEST(test_PBSD_jobfile)
  {
  enum job_file which = JScript;
  fail_unless(PBSD_jobfile(-1, 0, NULL, NULL, which) == PBSE_IVALREQ);
  fail_unless(PBSD_jobfile(PBS_NET_MAX_CONNECTIONS, 0, NULL, NULL, which) == PBSE_IVALREQ);
  }
END_TEST


START_TEST(test_PBSD_jscript)
  {
  fail_unless(PBSD_jscript(-1, NULL, NULL) == PBSE_IVALREQ);
  fail_unless(PBSD_jscript(PBS_NET_MAX_CONNECTIONS, NULL, NULL) == PBSE_IVALREQ);
  }
END_TEST


Suite *PBSD_submit_caps_suite(void)
  {
  Suite *s = suite_create("PBSD_submit_caps_suite methods");
  TCase *tc_core = tcase_create("test_PBSD_queuejob");
  tcase_add_test(tc_core, test_PBSD_queuejob);
  tcase_add_test(tc_core, test_PBSD_jobfile);
  tcase_add_test(tc_core, test_PBSD_jscript);
  suite_add_tcase(s, tc_core);

  tc_core = tcase_create("test_PBSD_QueueJob_hash");
  tcase_add_test(tc_core, test_PBSD_QueueJob_hash);
  tcase_add_test(tc_core, test_PBSD_scbuf);
  tcase_add_test(tc_core, test_PBSD_commit);
  tcase_add_test(tc_core, test_PBSD_commit_get_sid);
  tcase_add_test(tc_core, test_PBSD_rdytocmt);
  suite_add_tcase(s, tc_core);

  return s;
  }

void rundebug()
  {
  }

int main(void)
  {
  int number_failed = 0;
  SRunner *sr = NULL;
  rundebug();
  sr = srunner_create(PBSD_submit_caps_suite());
  srunner_set_log(sr, "PBSD_submit_caps_suite.log");
  srunner_run_all(sr, CK_NORMAL);
  number_failed = srunner_ntests_failed(sr);
  srunner_free(sr);
  return number_failed;
  }