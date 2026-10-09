// SPDX-License-Identifier: GPL-2.0-only
/*
 * Non-exclusive debugfs companion for the Logitech C-U0019 receiver.
 * Mouse input remains owned by hid-generic and usbhid.
 */

#include <linux/array_size.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/hid.h>
#include <linux/ktime.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/pm_runtime.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/usb.h>

#include "hid-logitech-cu0019.h"

static bool enable_reads;
module_param(enable_reads, bool, 0400);
MODULE_PARM_DESC(enable_reads,
		 "Enable fixed register reads on demand (default: false)");

enum cu0019_file_kind {
	CU0019_IDENTITY,
	CU0019_RADIO,
	CU0019_REGISTERS,
	CU0019_DIAGNOSTICS,
	CU0019_FILE_COUNT,
};

struct cu0019_sample {
	u8 dma[4];
	u8 rf_config[4];
	u8 tick_before[4];
	u8 tick_after[4];
	u8 pll[4];
	u8 rssi[4];
};

struct cu0019_snapshot {
	enum cu0019_file_kind kind;
	char device[64];
	u16 bcd_device;
	bool valid;
	bool quarantined;
	int error;
	const char *status;
	u64 attempted_ns;
	u64 duration_ns;
	u64 opened_ns;
	u64 refresh_attempts;
	u64 read_commands;
	u64 read_errors;
	u64 skipped_reads;
	u64 cache_hits;
	struct cu0019_sample sample;
};

struct cu0019_receiver;

struct cu0019_file {
	struct cu0019_receiver *receiver;
	enum cu0019_file_kind kind;
};

struct cu0019_receiver {
	struct list_head node;
	struct hid_device *hdev;
	struct usb_interface *intf;
	struct usb_device *udev;
	struct mutex lock; /* Protects the cache and vendor transactions. */
	struct dentry *directory;
	struct cu0019_file files[CU0019_FILE_COUNT];
	struct cu0019_snapshot cache;
	u64 next_attempt_ns;
	bool stopping;
	/* HID control transfers require a heap-backed buffer. */
	u8 transfer[CU0019_REPORT_SIZE];
};

static struct dentry *cu0019_debugfs_root;
static LIST_HEAD(cu0019_receivers);
static DEFINE_MUTEX(cu0019_receivers_lock);

/* Caller holds receiver->lock, transport device locks and an active PM ref. */
static int cu0019_read_register(struct cu0019_receiver *receiver, u16 address,
				u8 *data)
{
	int ret;

	ret = cu0019_build_read_request(address, receiver->transfer);
	if (ret)
		return ret;

	receiver->cache.read_commands++;
	ret = hid_hw_raw_request(receiver->hdev, CU0019_REPORT_ID,
				 receiver->transfer, CU0019_REPORT_SIZE,
				 HID_FEATURE_REPORT, HID_REQ_SET_REPORT);
	if (ret < 0)
		return ret;
	if (ret != CU0019_REPORT_SIZE)
		return -EPROTO;

	memset(receiver->transfer, 0, sizeof(receiver->transfer));
	receiver->transfer[0] = CU0019_REPORT_ID;
	ret = hid_hw_raw_request(receiver->hdev, CU0019_REPORT_ID,
				 receiver->transfer, CU0019_REPORT_SIZE,
				 HID_FEATURE_REPORT, HID_REQ_GET_REPORT);
	if (ret < 0)
		return ret;
	if (!cu0019_response_valid(receiver->transfer, ret))
		return -EPROTO;

	memcpy(data, receiver->transfer, 4);
	return 0;
}

static int cu0019_collect(struct cu0019_receiver *receiver,
			  struct cu0019_sample *sample)
{
	u8 check[4];
	int ret;

	ret = cu0019_read_register(receiver, CU0019_REG_DMA, sample->dma);
	if (ret)
		return ret;
	if (!cu0019_dma_valid(sample->dma))
		return -EPROTO;
	ret = cu0019_read_register(receiver, CU0019_REG_RF_CONFIG,
				   sample->rf_config);
	if (ret)
		return ret;
	if (!cu0019_rf_config_valid(sample->rf_config))
		return -EPROTO;
	ret = cu0019_read_register(receiver, CU0019_REG_TICK,
				   sample->tick_before);
	if (ret)
		return ret;
	ret = cu0019_read_register(receiver, CU0019_REG_PLL, sample->pll);
	if (ret)
		return ret;
	if (!cu0019_pll_valid(sample->pll))
		return -EPROTO;
	ret = cu0019_read_register(receiver, CU0019_REG_RSSI, sample->rssi);
	if (ret)
		return ret;
	ret = cu0019_read_register(receiver, CU0019_REG_TICK,
				   sample->tick_after);
	if (ret)
		return ret;
	ret = cu0019_read_register(receiver, CU0019_REG_DMA, check);
	if (ret)
		return ret;
	if (!cu0019_dma_valid(check))
		return -EPROTO;
	ret = cu0019_read_register(receiver, CU0019_REG_RF_CONFIG, check);
	if (ret)
		return ret;
	return cu0019_rf_config_valid(check) ? 0 : -EPROTO;
}

/* Trylocks avoid waiting for unbind, which can wait for a debugfs open. */
static bool cu0019_lock_transport(struct cu0019_receiver *receiver)
{
	if (!device_trylock(&receiver->udev->dev))
		return false;
	if (!device_trylock(&receiver->intf->dev))
		goto unlock_usb;
	if (!device_trylock(&receiver->hdev->dev))
		goto unlock_interface;
	return true;

unlock_interface:
	device_unlock(&receiver->intf->dev);
unlock_usb:
	device_unlock(&receiver->udev->dev);
	return false;
}

static void cu0019_unlock_transport(struct cu0019_receiver *receiver)
{
	device_unlock(&receiver->hdev->dev);
	device_unlock(&receiver->intf->dev);
	device_unlock(&receiver->udev->dev);
}

static int cu0019_validate_transport(struct cu0019_receiver *receiver)
{
	struct cu0019_snapshot *snapshot = &receiver->cache;
	struct hid_device *hdev = receiver->hdev;
	struct hid_report *feature;

	if (READ_ONCE(receiver->udev->state) == USB_STATE_SUSPENDED) {
		snapshot->status = "usb_not_active";
		return -EAGAIN;
	}
	if (READ_ONCE(receiver->udev->state) != USB_STATE_CONFIGURED ||
	    !receiver->intf->dev.driver ||
	    strcmp(receiver->intf->dev.driver->name, "usbhid") ||
	    !hdev->dev.driver ||
	    strcmp(hdev->dev.driver->name, "hid-generic")) {
		snapshot->status = "transport_unavailable";
		return -ENODEV;
	}
	feature = hdev->report_enum[HID_FEATURE_REPORT]
			  .report_id_hash[CU0019_REPORT_ID];
	if (!feature || feature->size != 56 || !hdev->ll_driver ||
	    !hdev->ll_driver->raw_request) {
		snapshot->status = "unexpected_feature_report";
		snapshot->quarantined = true;
		return -EPROTO;
	}
	return 0;
}

static void cu0019_refresh(struct cu0019_receiver *receiver)
{
	struct cu0019_snapshot *snapshot = &receiver->cache;
	u64 started_ns = ktime_get_ns();
	bool pm_reference = false;
	int ret;

	if (!enable_reads || receiver->stopping || snapshot->quarantined)
		return;
	if (started_ns < receiver->next_attempt_ns) {
		snapshot->cache_hits++;
		return;
	}

	snapshot->valid = false;
	snapshot->refresh_attempts++;
	snapshot->attempted_ns = started_ns;
	memset(&snapshot->sample, 0, sizeof(snapshot->sample));
	if (snapshot->bcd_device != CU0019_BCD_DEVICE) {
		snapshot->status = "unsupported_firmware";
		ret = -EOPNOTSUPP;
		goto complete;
	}
	if (!cu0019_lock_transport(receiver)) {
		snapshot->status = "device_busy";
		ret = -EBUSY;
		goto complete;
	}

	ret = cu0019_validate_transport(receiver);
	if (ret)
		goto unlock;
	if (pm_runtime_enabled(&receiver->udev->dev)) {
		ret = pm_runtime_get_if_active(&receiver->udev->dev);
		if (ret <= 0) {
			snapshot->status = "usb_not_active";
			ret = -EAGAIN;
			goto unlock;
		}
		pm_reference = true;
	}

	ret = cu0019_collect(receiver, &snapshot->sample);
	if (ret) {
		snapshot->status = "read_failed";
		snapshot->quarantined = true;
	} else {
		snapshot->status = "ok";
		snapshot->valid = true;
	}
	if (pm_reference)
		pm_runtime_put_noidle(&receiver->udev->dev);
unlock:
	cu0019_unlock_transport(receiver);
complete:
	snapshot->error = ret;
	if (ret) {
		if (snapshot->quarantined)
			snapshot->read_errors++;
		else
			snapshot->skipped_reads++;
	}
	snapshot->duration_ns = ktime_get_ns() - started_ns;
	receiver->next_attempt_ns = ktime_get_ns() + NSEC_PER_SEC;
}

static int cu0019_show(struct seq_file *file, void *unused)
{
	const struct cu0019_snapshot *snapshot = file->private;
	const struct cu0019_sample *sample = &snapshot->sample;

	if (snapshot->kind == CU0019_IDENTITY) {
		seq_printf(file, "device=%s\n", snapshot->device);
		seq_puts(file, "usb_id=046d:c542\n");
		seq_printf(file, "bcd_device=0x%04x\n", snapshot->bcd_device);
		seq_printf(file, "reads_enabled=%u\n", enable_reads);
		seq_printf(file, "supported_firmware=%u\n",
			   snapshot->bcd_device == CU0019_BCD_DEVICE);
		return 0;
	}
	if (snapshot->kind == CU0019_DIAGNOSTICS) {
		seq_printf(file, "last_status=%s\nlast_error=%d\n",
			   snapshot->status, snapshot->error);
		seq_printf(file, "quarantined=%u\n", snapshot->quarantined);
		seq_printf(file, "refresh_attempts=%llu\nread_commands=%llu\n",
			   snapshot->refresh_attempts, snapshot->read_commands);
		seq_printf(file, "read_errors=%llu\nskipped_reads=%llu\n",
			   snapshot->read_errors, snapshot->skipped_reads);
		seq_printf(file, "cache_hits=%llu\n", snapshot->cache_hits);
		seq_printf(file, "last_attempt_monotonic_ns=%llu\n",
			   snapshot->attempted_ns);
		seq_printf(file, "last_attempt_age_at_open_ms=%llu\n",
			   snapshot->attempted_ns ? (snapshot->opened_ns -
						     snapshot->attempted_ns) /
							    NSEC_PER_MSEC :
						    0);
		seq_printf(file, "last_attempt_duration_us=%llu\n",
			   snapshot->duration_ns / NSEC_PER_USEC);
		seq_puts(file, "minimum_poll_interval_ms=1000\n");
		return 0;
	}

	seq_printf(file, "status=%s\n", snapshot->status);
	if (!snapshot->valid)
		return 0;
	if (snapshot->kind == CU0019_RADIO) {
		seq_printf(file, "rssi_raw=%u\n", sample->rssi[0]);
		seq_printf(file, "frequency_mhz_candidate=%u\n",
			   get_unaligned_le16(sample->pll + 2));
		return 0;
	}
	seq_printf(file, "0x0508=%*ph\n0x0438=%*ph\n", 4, sample->dma, 4,
		   sample->rf_config);
	seq_printf(file, "0x0740_before=%*ph\n0x0740_after=%*ph\n", 4,
		   sample->tick_before, 4, sample->tick_after);
	seq_printf(file, "0x04d4=%*ph\n0x0458=%*ph\n", 4, sample->pll, 4,
		   sample->rssi);
	return 0;
}

static int cu0019_open(struct inode *inode, struct file *file)
{
	const struct cu0019_file *entry = inode->i_private;
	struct cu0019_receiver *receiver = entry->receiver;
	struct cu0019_snapshot *snapshot;
	int ret;

	snapshot = kzalloc_obj(*snapshot);
	if (!snapshot)
		return -ENOMEM;

	mutex_lock(&receiver->lock);
	if (receiver->stopping) {
		ret = -ENODEV;
		goto unlock_free;
	}
	if (entry->kind == CU0019_RADIO || entry->kind == CU0019_REGISTERS)
		cu0019_refresh(receiver);
	*snapshot = receiver->cache;
	snapshot->kind = entry->kind;
	snapshot->opened_ns = ktime_get_ns();
	mutex_unlock(&receiver->lock);

	/* Reads, seeks and release only access this independent per-open copy. */
	ret = single_open(file, cu0019_show, snapshot);
	if (!ret)
		return 0;
	goto free_snapshot;

unlock_free:
	mutex_unlock(&receiver->lock);
free_snapshot:
	kfree(snapshot);
	return ret;
}

static int cu0019_release(struct inode *inode, struct file *file)
{
	struct seq_file *seq = file->private_data;
	struct cu0019_snapshot *snapshot = seq->private;
	int ret;

	ret = single_release(inode, file);
	kfree(snapshot);
	return ret;
}

static const struct file_operations cu0019_file_operations = {
	.owner = THIS_MODULE,
	.open = cu0019_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = cu0019_release,
};

static void cu0019_destroy(struct cu0019_receiver *receiver)
{
	mutex_lock(&receiver->lock);
	receiver->stopping = true;
	mutex_unlock(&receiver->lock);

	/* The full debugfs proxy drains opens; never hold their mutex here. */
	debugfs_remove(receiver->directory);
	put_device(&receiver->hdev->dev);
	usb_put_intf(receiver->intf);
	usb_put_dev(receiver->udev);
	kfree(receiver);
}

static int cu0019_add(struct device *device, struct subsys_interface *interface)
{
	static const char *const names[] = {
		"identity",
		"radio",
		"registers",
		"diagnostics",
	};
	struct hid_device *hdev = to_hid_device(device);
	struct cu0019_receiver *receiver;
	struct dentry *entry;
	unsigned int i;
	int ret = 0;

	if (hdev->bus != BUS_USB || hdev->vendor != CU0019_VENDOR_ID ||
	    hdev->product != CU0019_PRODUCT_ID || !hid_is_usb(hdev))
		return 0;

	receiver = kzalloc_obj(*receiver);
	if (!receiver)
		return -ENOMEM;
	receiver->hdev = to_hid_device(get_device(device));
	receiver->intf = usb_get_intf(to_usb_interface(device->parent));
	receiver->udev = usb_get_dev(interface_to_usbdev(receiver->intf));
	mutex_init(&receiver->lock);
	strscpy(receiver->cache.device, dev_name(device),
		sizeof(receiver->cache.device));
	receiver->cache.bcd_device =
		le16_to_cpu(receiver->udev->descriptor.bcdDevice);
	receiver->cache.status = enable_reads ? "not_sampled" :
						"reads_disabled";
	receiver->directory =
		debugfs_create_dir(dev_name(device), cu0019_debugfs_root);
	if (IS_ERR(receiver->directory)) {
		ret = PTR_ERR(receiver->directory);
		receiver->directory = NULL;
		goto destroy_receiver;
	}
	for (i = 0; i < ARRAY_SIZE(names); i++) {
		receiver->files[i].receiver = receiver;
		receiver->files[i].kind = i;
		entry = debugfs_create_file(names[i], 0400, receiver->directory,
					    &receiver->files[i],
					    &cu0019_file_operations);
		if (IS_ERR(entry)) {
			ret = PTR_ERR(entry);
			goto destroy_receiver;
		}
	}
	mutex_lock(&cu0019_receivers_lock);
	list_add_tail(&receiver->node, &cu0019_receivers);
	mutex_unlock(&cu0019_receivers_lock);
	return 0;

destroy_receiver:
	cu0019_destroy(receiver);
	dev_dbg(device, "Cannot create CU0019 debugfs files: %d\n", ret);
	return ret;
}

static void cu0019_remove(struct device *device,
			  struct subsys_interface *interface)
{
	struct cu0019_receiver *receiver;
	struct cu0019_receiver *next;

	mutex_lock(&cu0019_receivers_lock);
	list_for_each_entry_safe(receiver, next, &cu0019_receivers, node) {
		if (&receiver->hdev->dev != device)
			continue;
		list_del(&receiver->node);
		mutex_unlock(&cu0019_receivers_lock);
		cu0019_destroy(receiver);
		return;
	}
	mutex_unlock(&cu0019_receivers_lock);
}

static struct subsys_interface cu0019_interface = {
	.name = "hid-logitech-cu0019-debug",
	.subsys = &hid_bus_type,
	.add_dev = cu0019_add,
	.remove_dev = cu0019_remove,
};

static int __init cu0019_init(void)
{
	int ret;

	cu0019_debugfs_root = debugfs_create_dir("cu0019", NULL);
	if (IS_ERR(cu0019_debugfs_root))
		return PTR_ERR(cu0019_debugfs_root);
	ret = subsys_interface_register(&cu0019_interface);
	if (ret)
		debugfs_remove(cu0019_debugfs_root);
	return ret;
}

static void __exit cu0019_exit(void)
{
	subsys_interface_unregister(&cu0019_interface);
	debugfs_remove(cu0019_debugfs_root);
}

module_init(cu0019_init);
module_exit(cu0019_exit);

MODULE_AUTHOR("Jerome Tollet <jerome.tollet@gmail.com>");
MODULE_DESCRIPTION("Read-only radio debugfs companion for Logitech C-U0019");
MODULE_LICENSE("GPL");
