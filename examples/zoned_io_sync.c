// Copyright (C) Simon A. F. Lund <simon.lund@samsung.com>
// SPDX-License-Identifier: Apache-2.0
#include <stdio.h>
#include <errno.h>
#include <libxnvme.h>
#include <libxnvmec.h>
#include <libxnvme_util.h>
#include <libznd.h>
#include <time.h>

static int
sub_sync_read(struct xnvmec *cli)
{
	struct xnvme_dev *dev = cli->args.dev;
	const struct xnvme_geo *geo = cli->args.geo;
	uint32_t nsid = cli->args.nsid;

	struct znd_descr zone = { 0 };

	int cmd_opts = XNVME_CMD_SYNC;

	size_t buf_nbytes;
	char *buf = NULL;
	int err;

	if (!cli->given[XNVMEC_OPT_NSID]) {
		nsid = xnvme_dev_get_nsid(cli->args.dev);
	}
	if (cli->given[XNVMEC_OPT_SLBA]) {
		err = znd_descr_from_dev(dev, cli->args.slba, &zone);
		if (err) {
			xnvmec_perr("znd_descr_from_dev()", -err);
			goto exit;
		}
	} else {
		err = znd_descr_from_dev_in_state(dev, ZND_STATE_EMPTY, &zone);
		if (err) {
			xnvmec_perr("znd_descr_from_dev()", -err);
			goto exit;
		}
	}
	xnvmec_pinf("Using the following zone:");
	znd_descr_pr(&zone, XNVME_PR_DEF);

	buf_nbytes = zone.zcap * geo->lba_nbytes;

	xnvmec_pinf("Allocating and filling buf_nbytes: %zu", buf_nbytes);
	buf = xnvme_buf_alloc(dev, buf_nbytes, NULL);
	if (!buf) {
		err = -errno;
		xnvmec_perr("xnvme_buf_alloc()", err);
		goto exit;
	}
	err = xnvmec_buf_fill(buf, buf_nbytes, "zero");
	if (err) {
		xnvmec_perr("xnvmec_buf_fill()", err);
		goto exit;
	}

	xnvmec_pinf("Read from uri: '%s'", cli->args.uri);
	xnvmec_timer_start(cli);

	for (uint64_t sect = 0; sect < zone.zcap; ++sect) {
		void *payload = buf + sect * geo->lba_nbytes;
		struct xnvme_req req = { 0 };

		err = xnvme_cmd_read(dev, nsid, zone.zslba + sect, 0, payload,
				     NULL, cmd_opts, &req);
		if (err || xnvme_req_cpl_status(&req)) {
			xnvmec_perr("xnvme_cmd_read()", err);
			xnvme_req_pr(&req, XNVME_PR_DEF);
			err = err ? err : -EIO;
			goto exit;
		}
	}

	xnvmec_timer_stop(cli);
	xnvmec_timer_bw_pr(cli, "Wall-clock", zone.zcap * geo->lba_nbytes);

	if (cli->args.data_output) {
		xnvmec_pinf("Dumping nbytes: %zu, to: '%s'",
			    buf_nbytes, cli->args.data_output);
		err = xnvmec_buf_to_file(buf, buf_nbytes,
					 cli->args.data_output);
		if (err) {
			xnvmec_perr("xnvmec_buf_to_file()", err);
		}
	}

exit:
	xnvme_buf_free(dev, buf);

	return err < 0 ? err : 0;
}

static int
sub_sync_write(struct xnvmec *cli)
{
	struct xnvme_dev *dev = cli->args.dev;
	const struct xnvme_geo *geo = cli->args.geo;
	uint32_t nsid = cli->args.nsid;

	struct znd_descr zone = { 0 };

	int cmd_opts = XNVME_CMD_SYNC;

	size_t buf_nbytes;
	char *buf = NULL;
	int err;

	if (!cli->given[XNVMEC_OPT_NSID]) {
		nsid = xnvme_dev_get_nsid(cli->args.dev);
	}
	if (cli->given[XNVMEC_OPT_SLBA]) {
		err = znd_descr_from_dev(dev, cli->args.slba, &zone);
		if (err) {
			xnvmec_perr("znd_descr_from_dev()", -err);
			goto exit;
		}
	} else {
		err = znd_descr_from_dev_in_state(dev, ZND_STATE_EMPTY, &zone);
		if (err) {
			xnvmec_perr("znd_descr_from_dev()", -err);
			goto exit;
		}
	}
	xnvmec_pinf("Using the following zone:");
	znd_descr_pr(&zone, XNVME_PR_DEF);

	buf_nbytes = zone.zcap * geo->lba_nbytes;

	xnvmec_pinf("Allocating and filling buf_nbytes: %zu", buf_nbytes);
	buf = xnvme_buf_alloc(dev, buf_nbytes, NULL);
	if (!buf) {
		err = -errno;
		xnvmec_perr("xnvme_buf_alloc()", err);
		goto exit;
	}
	err = xnvmec_buf_fill(buf, buf_nbytes, cli->args.data_input ? cli->args.data_input : "anum");
	if (err) {
		xnvmec_perr("xnvmec_buf_fill()", err);
		goto exit;
	}

	xnvmec_pinf("Write to uri: '%s'", cli->args.uri);
	xnvmec_timer_start(cli);

	for (uint64_t sect = 0; sect < zone.zcap; ++sect) {
		void *payload = buf + sect * geo->lba_nbytes;
		struct xnvme_req req = { 0 };

		err = xnvme_cmd_write(dev, nsid, zone.zslba + sect, 0, payload,
				      NULL, cmd_opts, &req);
		if (err || xnvme_req_cpl_status(&req)) {
			xnvmec_perr("xnvme_cmd_append()", err);
			xnvme_req_pr(&req, XNVME_PR_DEF);
			err = err ? err : -EIO;
			goto exit;
		}
	}

	xnvmec_timer_stop(cli);
	xnvmec_timer_bw_pr(cli, "Wall-clock", zone.zcap * geo->lba_nbytes);

exit:
	xnvme_buf_free(dev, buf);

	return err < 0 ? err : 0;
}

static int
sub_sync_append(struct xnvmec *cli)
{
	struct xnvme_dev *dev = cli->args.dev;
	const struct xnvme_geo *geo = cli->args.geo;
	uint32_t nsid = cli->args.nsid;

	struct znd_descr zone = { 0 };

	int cmd_opts = XNVME_CMD_SYNC;

	size_t buf_nbytes;
	char *buf = NULL;
	int err;

	if (!cli->given[XNVMEC_OPT_NSID]) {
		nsid = xnvme_dev_get_nsid(cli->args.dev);
	}
	if (cli->given[XNVMEC_OPT_SLBA]) {
		err = znd_descr_from_dev(dev, cli->args.slba, &zone);
		if (err) {
			xnvmec_perr("znd_descr_from_dev()", -err);
			goto exit;
		}
	} else {
		err = znd_descr_from_dev_in_state(dev, ZND_STATE_EMPTY, &zone);
		if (err) {
			xnvmec_perr("znd_descr_from_dev()", -err);
			goto exit;
		}
	}
	xnvmec_pinf("Using the following zone:");
	znd_descr_pr(&zone, XNVME_PR_DEF);

	buf_nbytes = zone.zcap * geo->lba_nbytes;

	xnvmec_pinf("Allocating and filling buf_nbytes: %zu", buf_nbytes);
	buf = xnvme_buf_alloc(dev, buf_nbytes, NULL);
	if (!buf) {
		err = -errno;
		xnvmec_perr("xnvme_buf_alloc()", err);
		goto exit;
	}
	err = xnvmec_buf_fill(buf, buf_nbytes, cli->args.data_input ? cli->args.data_input : "anum");
	if (err) {
		xnvmec_perr("xnvmec_buf_fill()", err);
		goto exit;
	}

	xnvmec_pinf("Append to uri: '%s'", cli->args.uri);
	xnvmec_timer_start(cli);

	for (uint64_t sect = 0; sect < zone.zcap; ++sect) {
		void *payload = buf + sect * geo->lba_nbytes;
		struct xnvme_req req = { 0 };

		err = znd_cmd_append(dev, nsid, zone.zslba, 0, payload,
				     NULL, cmd_opts, &req);
		if (err || xnvme_req_cpl_status(&req)) {
			xnvmec_perr("xnvme_cmd_append()", err);
			xnvme_req_pr(&req, XNVME_PR_DEF);
			err = err ? err : -EIO;
			goto exit;
		}
	}

	xnvmec_timer_stop(cli);
	xnvmec_timer_bw_pr(cli, "Wall-clock", zone.zcap * geo->lba_nbytes);

exit:
	xnvme_buf_free(dev, buf);

	return err < 0 ? err : 0;
}

//
// Command-Line Interface (CLI) definition
//

static struct xnvmec_sub g_subs[] = {
	{
		"read", "Zone Read of a full zone",
		"Zone Read of a full zone", sub_sync_read, {
			{XNVMEC_OPT_URI, XNVMEC_POSA},
			{XNVMEC_OPT_SLBA, XNVMEC_LOPT},
			{XNVMEC_OPT_DATA_OUTPUT, XNVMEC_LOPT},
		}
	},

	{
		"write", "Zone Write sync. until full",
		"Zone Write sync. until full", sub_sync_write, {
			{XNVMEC_OPT_URI, XNVMEC_POSA},
			{XNVMEC_OPT_SLBA, XNVMEC_LOPT},
			{XNVMEC_OPT_DATA_INPUT, XNVMEC_LOPT},
		}
	},

	{
		"append", "Zone Append sync. until full",
		"Zone Append sync. until full", sub_sync_append, {
			{XNVMEC_OPT_URI, XNVMEC_POSA},
			{XNVMEC_OPT_SLBA, XNVMEC_LOPT},
			{XNVMEC_OPT_DATA_INPUT, XNVMEC_LOPT},
		}
	},
};

static struct xnvmec g_cli = {
	.title = "Zoned Synchronous IO Example",
	.descr_short =	"Synchronous IO: read / write / append, "
	"using 4k payload at QD1",
	.subs = g_subs,
	.nsubs = sizeof g_subs / sizeof(*g_subs),
};

int
main(int argc, char **argv)
{
	return xnvmec(&g_cli, argc, argv, XNVMEC_INIT_DEV_OPEN);
}
