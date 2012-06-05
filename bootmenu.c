/* 
 * Acer bootloader boot menu application main file.
 * 
 * Copyright (C) 2012 Skrilax_CZ
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <stddef.h>
#include <bl_0_03_14.h>
#include <bootmenu.h>
#include <framebuffer.h>

/* Boot menu items */
const char* boot_menu_items[] =
{
	"%s%sReboot",
	"%s%sFastboot Mode",
	"%s%sBoot Primary Kernel Image",
	"%s%sBoot Secondary Kernel Image",
	"%s%sBoot Recovery",
	"%s%sSet Boot %s Kernel Image",
	"%s%sSet Debug Mode %s",
	"%s%sWipe Cache",
};

/* Device keys */
struct gpio_key device_keys[] = 
{
	/* Volume UP */
	{
		.row = 16,
		.column = 4,
		.active_low = 1,
	},
	/* Volume DOWN */
	{
		.row = 16,
		.column = 5,
		.active_low = 1,
	},
	/* Rotation Lock */
	{
		.row = 16,
		.column = 2,
		.active_low = 1,
	},
	/* Power */
	{
		.row = 8,
		.column = 3,
		.active_low = 0,
	},
};

/* MSC partition command */
struct msc_command msc_cmd;

/* How to boot */
enum boot_mode this_boot_mode = BM_NORMAL;

/* How to boot from msc command */
enum boot_mode msc_boot_mode = BM_NORMAL;

/* Full bootloader version */
char full_bootloader_version[0x80];

/*
 * Is key active
 */
int get_key_active(enum key_type key)
{
	int gpio_state = get_gpio(device_keys[key].row, device_keys[key].column);
	
	if (device_keys[key].active_low)
		return gpio_state == 0;
	else
		return gpio_state == 1;
}

/* 
 * Wait for key event
 */
enum key_type wait_for_key_event()
{
	/* Wait for key event, debounce them first */
	while (get_key_active(KEY_VOLUME_UP))
		sleep(30);
	
	while (get_key_active(KEY_VOLUME_DOWN))
		sleep(30);
	
	while (get_key_active(KEY_POWER))
		sleep(30);
		
	while (1)
	{
		if (get_key_active(KEY_VOLUME_DOWN))
			return KEY_VOLUME_DOWN;
		
		if (get_key_active(KEY_VOLUME_UP))
			return KEY_VOLUME_UP;
		
		if (get_key_active(KEY_POWER))
		{
			/* Power key - act on releasing it */
			while (get_key_active(KEY_POWER))
				sleep(30);
			
			return KEY_POWER;
		}
		
		sleep(30);
	}
}

/*
 * Read MSC command
 */
void msc_cmd_read()
{
	struct msc_command my_cmd;
	int msc_pt_handle;
	int processed_bytes;
	
	msc_pt_handle = 0;
	
	if (open_partition("MSC", PARTITION_OPEN_READ, &msc_pt_handle))
		goto finish;
	
	if (read_partition(msc_pt_handle, &my_cmd, sizeof(my_cmd), &processed_bytes))
		goto finish;
	
	if (processed_bytes != sizeof(my_cmd))
		goto finish;
	
	memcpy(&msc_cmd, &my_cmd, sizeof(my_cmd));
		
finish:
	close_partition(msc_pt_handle);
	return;	
}

/*
 * Write MSC Command
 */
void msc_cmd_write()
{
	int msc_pt_handle;
	int processed_bytes;
	
	msc_pt_handle = 0;
	
	if (open_partition("MSC", PARTITION_OPEN_WRITE, &msc_pt_handle))
		goto finish;
	
	write_partition(msc_pt_handle, &msc_cmd, sizeof(msc_cmd), &processed_bytes);
	
finish:
	close_partition(msc_pt_handle);
	return;	
}

/*
 * Boot Android (returns on ERROR)
 */
void boot_android_image(const char* partition, int boot_magic_value)
{
	char* kernel_code = NULL;
	int kernel_ep = 0;
	
	if (!android_load_image(&kernel_code, &kernel_ep, partition))
		return;
	
	if (!*(kernel_code))
		return;
	
	android_boot_image(kernel_code, kernel_ep, boot_magic_value);
}

/*
 * Boots normally (returns on ERROR)
 */
void boot_normal(int boot_partition, int boot_magic_value)
{
	/* Normal mode frame */
	bootmenu_basic_frame();
	
	if (boot_partition == 0)
	{
		fb_set_status("Booting primary kernel image");
		fb_refresh();
		
		boot_android_image("LNX", boot_magic_value);
	}
	else
	{
		fb_set_status("Booting secondary kernel image");
		fb_refresh();
		
		boot_android_image("AKB", boot_magic_value);
	}
}

/*
 * Boots to recovery (returns on ERROR)
 */
void boot_recovery(int boot_magic_value)
{
	/* Normal mode frame */
	bootmenu_basic_frame();
	
	fb_set_status("Booting recovery kernel image");
	fb_refresh();
	boot_android_image("SOS", boot_magic_value);
}

/*
 * Bootmenu frame
 */
void bootmenu_frame(void)
{
	/* Centered */
	char buffer[TEXT_LINE_CHARS+1];
	const char* hint = "Use volume keys to highlight, power to select.";
	int i;
	int l = strlen(hint);
	
	/* clear screen */
	fb_clear();
	
	/* set status */
	fb_set_status("Bootmenu Mode");
	
	/* Draw hint */
	for (i = 0; i < (TEXT_LINE_CHARS - l) / 2; i++)
		buffer[i] = ' ';

	strncpy(&(buffer[(TEXT_LINE_CHARS - l) / 2]), hint, l);
	fb_printf(buffer);
	fb_printf("\n\n\n");
}

/*
 * Basic frame (normal mode)
 */
void bootmenu_basic_frame(void)
{
	/* clear screen */
	fb_clear();
	
	/* clear status */
	fb_set_status("");
}

/*
 * Bootmenu error
 */
void error(void)
{
	bootmenu_basic_frame();
	fb_printf("%sUnrecoverable bootloader error, please reboot the device manually.", fb_text_color_code(0xFF, 0xFF, 0x01));
	
	while (1)
		sleep(1000);
}

/* 
 * Entry point of bootmenu 
 * Magic is Acer BL data structure, used as parameter for some functions.
 * 
 * - keystate at boot is loaded
 * - boot partition (primary or secondary) is loaded
 * - can continue boot, and force fastboot or recovery mode
 * 
 * boot magic_boot_argument - pass to boot partition
 */
void main(void* magic, int magic_boot_argument)
{
	/* Selected option in boot menu */
	int selected_option = 0;
	
	/* Key press: -1 nothing, 0 Vol DOWN, 1 Vol UP, 2 Power */
	enum key_type key_press = KEY_NONE;

	/* Which kernel image is booted */
	const char* boot_partition_str;
	const char* other_boot_partition_str;
	
	/* Debug mode status */
	const char* debug_mode_str;
	const char* other_debug_mode_str;
	
	/* Print error, from which partition booting failed */
	const char* boot_partition_attempt = NULL;
	
	/* Line builder - two color codes used */
	char line_builder[TEXT_LINE_CHARS + 8 + 1];
	
	int i, l;
	const char* b;
	const char* c;
	
	/* Fill full bootloader version */
	snprintf(full_bootloader_version, 0x80, bootloader_id, bootloader_version);
	
	/* Initialize framebuffer */
	fb_init();
	
	/* Set title */
	fb_set_title(full_bootloader_version);
	
	/* Ensure we have bootloader update */
	check_bootloader_update(magic);
	
	/* Read msc command */
	msc_cmd_read();
	
	/* First, check MSC command */
	if (!strncmp((const char*)msc_cmd.boot_command, MSC_CMD_RECOVERY, strlen(MSC_CMD_RECOVERY)))
		this_boot_mode = BM_RECOVERY;
	else if (!strncmp((const char*)msc_cmd.boot_command, MSC_CMD_FCTRY_RESET, strlen(MSC_CMD_FCTRY_RESET)))
		this_boot_mode = BM_FCTRY_RESET;
	else if (!strncmp((const char*)msc_cmd.boot_command, MSC_CMD_FASTBOOT, strlen(MSC_CMD_FASTBOOT)))
		this_boot_mode = BM_FASTBOOT;
	else if (!strncmp((const char*)msc_cmd.boot_command, MSC_CMD_BOOTMENU, strlen(MSC_CMD_BOOTMENU)))
		this_boot_mode = BM_BOOTMENU;
	else
		this_boot_mode = BM_NORMAL;
	
	msc_boot_mode = this_boot_mode;
	
	/* Evaluate key status */
	if (get_key_active(KEY_VOLUME_UP))
		this_boot_mode = BM_BOOTMENU;
	else if (get_key_active(KEY_VOLUME_DOWN))
		this_boot_mode = BM_RECOVERY;
		
	/* Clear boot command from msc */
	memset(msc_cmd.boot_command, 0, ARRAY_SIZE(msc_cmd.boot_command));
	msc_cmd_write();
	
	/* Evaluate boot mode */
	if (this_boot_mode == BM_NORMAL)
	{
		if (msc_cmd.boot_partition == 0)
			boot_partition_attempt = "primary (LNX)";
		else
			boot_partition_attempt = "secondary (AKB)";
		
		boot_normal(msc_cmd.boot_partition, magic_boot_argument);
	}
	else if (this_boot_mode == BM_RECOVERY)
	{
		boot_partition_attempt = "recovery (SOS)";
		boot_recovery(magic_boot_argument);
	}
	else if (this_boot_mode == BM_FCTRY_RESET)
	{
		fb_set_status("Factory reset\n");
		
		/* Erase userdata */
		fb_printf("Erasing UDA partition...\n\n");
		fb_refresh();
		format_partition("UDA");
		 
		/* Erase cache */
		fb_printf("Erasing CAC partition...\n\n");
		fb_refresh();
		format_partition("CAC");
		
		/* Finished */
		fb_printf("Done.\n");
		fb_refresh();
		
		/* Sleep */
		sleep(5000);
		
		/* Reboot */
		reboot(magic);
		
		/* Reboot returned */
		error();
	}
	else if (this_boot_mode == BM_FASTBOOT)
	{
		/* Return -> jump to fastboot */
		return;
	}
	
	/* Allright - now we're in bootmenu */
	
	/* Boot menu */
	while (1)
	{ 
		/* New frame */
		bootmenu_frame();
		
		/* Print current boot mode */
		if (msc_cmd.boot_partition == 0)
		{
			boot_partition_str = "Primary";
			other_boot_partition_str = "Secondary";
		}
		else
		{
			boot_partition_str = "Secondary";
			other_boot_partition_str = "Primary";
		}
		
		fb_printf("Current boot mode: %s kernel image\n", boot_partition_str);
		
		if (msc_cmd.debug_mode == 0)
		{
			debug_mode_str = "OFF";
			other_debug_mode_str = "ON";
		}
		else
		{
			debug_mode_str = "ON";
			other_debug_mode_str = "OFF";
		}
		
		fb_printf("Debug mode: %s\n\n", debug_mode_str);
		
		/* Print error if we're stuck in bootmenu */
		if (boot_partition_attempt)
			fb_printf("%sERROR: Invalid %s kernel image.\n\n", fb_text_color_code(0xFF, 0xFF, 0x01), boot_partition_attempt);
		
		fb_printf("\n");
		
		/* Print options */
		for (i = 0; i < ARRAY_SIZE(boot_menu_items); i++)
		{
			memset(line_builder, 0x20, ARRAY_SIZE(line_builder));
			line_builder[ARRAY_SIZE(line_builder) - 1] = '\0';
			line_builder[ARRAY_SIZE(line_builder) - 2] = '\n';
			
			if (i == selected_option)
			{
				b = fb_background_color_code(text_color.R, text_color.G, text_color.B);
				c = fb_text_color_code(highlight_color.R, highlight_color.G, highlight_color.B);
			}
			else
			{
				b = fb_background_color_code(0, 0, 0);
				c = fb_text_color_code(text_color.R, text_color.G, text_color.B);
			}
			
			if (i == 5)
				snprintf(line_builder, ARRAY_SIZE(line_builder), boot_menu_items[i], b, c, other_boot_partition_str);
			else if (i == 6)
				snprintf(line_builder, ARRAY_SIZE(line_builder), boot_menu_items[i], b, c, other_debug_mode_str);
			else
				snprintf(line_builder, ARRAY_SIZE(line_builder), boot_menu_items[i], b, c);
			
			l = strlen(line_builder);
			if (l == ARRAY_SIZE(line_builder) - 1)
				line_builder[ARRAY_SIZE(line_builder) - 2] = '\n';
			else if (l < ARRAY_SIZE(line_builder) - 1)
				line_builder[l] = ' ';
			
			fb_printf(line_builder);
		}
		
		/* Draw framebuffer */
		fb_refresh();
		
		key_press = wait_for_key_event();
		
		if (key_press == KEY_NONE)
			continue;
		
		/* Volume DOWN */
		if (key_press == KEY_VOLUME_DOWN)
		{
			selected_option++;
			
			if (selected_option >= ARRAY_SIZE(boot_menu_items))
				selected_option = 0;
			
			continue;
		}
		
		/* Volume UP */
		if (key_press == KEY_VOLUME_UP)
		{
			selected_option--;
			
			if (selected_option < 0)
				selected_option = ARRAY_SIZE(boot_menu_items) - 1;
			
			continue;
		}
		
		/* Power */
		if (key_press == KEY_POWER)
		{
			switch(selected_option)
			{
				case 0: /* Reboot */
					reboot(magic);
					
					/* Reboot returned */
					error();
					
				case 1: /* Fastboot mode */
					return;
				
				case 2: /* Primary kernel image */
					boot_partition_attempt = "primary (LNX)";
					boot_normal(0, magic_boot_argument);
					break;
					
				case 3: /* Secondary kernel image */
					boot_partition_attempt = "secondary (AKB)";
					boot_normal(1, magic_boot_argument);
					break;
					
				case 4: /* Recovery kernel image */
					boot_partition_attempt = "recovery (SOS)";
					boot_recovery(magic_boot_argument);
					break;
					
				case 5: /* Toggle boot kernel image */
					msc_cmd.boot_partition = !msc_cmd.boot_partition;
					msc_cmd_write();
					selected_option = 0;
					break;
					
				case 6: /* Toggle debug mode */
					msc_cmd.debug_mode = !msc_cmd.debug_mode;
					msc_cmd_write();
					selected_option = 0;
					break;
					
				case 7: /* Wipe cache */
					bootmenu_basic_frame();
					fb_set_status("Bootmenu Mode");
					fb_printf("Erasing CAC partition...\n\n");
					fb_refresh();
					
					format_partition("CAC");
					
					fb_printf("Done.\n");
					fb_refresh();
					sleep(2000);
					
					selected_option = 0;
					break;
			}
		}
		
	}
}
