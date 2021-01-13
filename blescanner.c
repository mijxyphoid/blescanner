//
//  Intel Edison Playground
//  Copyright (c) 2015 Damian Kołakowski. All rights reserved.
//	Modified by Matti Jones 2020 Added UUID, Major and Minor an TX Power Reporting
//

#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

struct hci_request ble_hci_request(uint16_t ocf, int clen, void * status, void * cparam)
{
	struct hci_request rq;
	memset(&rq, 0, sizeof(rq));
	rq.ogf = OGF_LE_CTL;
	rq.ocf = ocf;
	rq.cparam = cparam;
	rq.clen = clen;
	rq.rparam = status;
	rq.rlen = 1;
	return rq;
}

int main()
{
	int ret, status;

	// Get HCI device.

	int device = 0;																	// Declare Variable for our bluetooth receiver device ID
	device = hci_open_dev(hci_get_route(NULL));										// Attempt to open our bluetooth receiver device
	if ( device < 0 ) 																// If there was an error opening our bluetooth device
	{
		char system_command[32];													// Stores our System Command we wish to execute
		int device_id = hci_get_route(NULL);										// Get the HCI Device ID (hci#)
		if (device_id < 0)
		{
			perror("Could not locate a HCI device.");
			return 0; 
		}
		else
		{
			printf("Bringing HCI Device hci%i Down...\n", device_id);				// Debug Message - Bringing Bluetooth receiver down
			sprintf(system_command, "hciconfig hci%i down", device_id);				// Build our system command to shut down our HCI device
			system(system_command);													// Execute our System Command
			sleep (1);																// Sleep for 1 second so as everything stabilises
			printf("Bringing HCI Device hci%i Up...\n", device_id);					// Debug Message - Bringing Bluetooth receiver down
			sprintf(system_command, "hciconfig hci%i up", device_id);				// Build our system command to restart our HCI device
			system(system_command);													// Execute our System Command
			sleep (1);																// Sleep for 1 second so as everything stabilises
			
			device = hci_open_dev(hci_get_route(NULL));								// Attempt to open our bluetooth receiver device again
			if ( device < 0 ) 														// If there was an error opening our bluetooth device
			{
				perror("Failed to open HCI device.");
				return 0; 
			}
		}
	}

	// Set BLE scan parameters.
	
	le_set_scan_parameters_cp scan_params_cp;
	memset(&scan_params_cp, 0, sizeof(scan_params_cp));
	scan_params_cp.type 			= 0x00; 
	scan_params_cp.interval 		= htobs(0x0010);
	scan_params_cp.window 			= htobs(0x0010);
	scan_params_cp.own_bdaddr_type 	= 0x00; 										// Public Device Address (default).
	scan_params_cp.filter 			= 0x00; 										// Accept all.

	struct hci_request scan_params_rq = ble_hci_request(OCF_LE_SET_SCAN_PARAMETERS, LE_SET_SCAN_PARAMETERS_CP_SIZE, &status, &scan_params_cp);
	
	ret = hci_send_req(device, &scan_params_rq, 1000);
	if ( ret < 0 ) 
	{
		
		hci_close_dev(device);
		perror("Failed to set scan parameters data.");
		return 0;
	}

	// Set BLE events report mask.

	le_set_event_mask_cp event_mask_cp;
	memset(&event_mask_cp, 0, sizeof(le_set_event_mask_cp));
	int i = 0;
	for ( i = 0 ; i < 8 ; i++ ) event_mask_cp.mask[i] = 0xFF;

	struct hci_request set_mask_rq = ble_hci_request(OCF_LE_SET_EVENT_MASK, LE_SET_EVENT_MASK_CP_SIZE, &status, &event_mask_cp);
	ret = hci_send_req(device, &set_mask_rq, 1000);
	if ( ret < 0 ) 
	{
		hci_close_dev(device);
		perror("Failed to set event mask.");
		return 0;
	}

	// Enable scanning.

	le_set_scan_enable_cp scan_cp;
	memset(&scan_cp, 0, sizeof(scan_cp));
	scan_cp.enable 		= 0x01;														// Enable flag.
	scan_cp.filter_dup 	= 0x00; 													// Filtering disabled.

	struct hci_request enable_adv_rq = ble_hci_request(OCF_LE_SET_SCAN_ENABLE, LE_SET_SCAN_ENABLE_CP_SIZE, &status, &scan_cp);

	ret = hci_send_req(device, &enable_adv_rq, 1000);
	if ( ret < 0 ) 
	{
		hci_close_dev(device);
		perror("Failed to enable scan.");
		return 0;
	}

	// Get Results.

	struct hci_filter nf;
	hci_filter_clear(&nf);
	hci_filter_set_ptype(HCI_EVENT_PKT, &nf);
	hci_filter_set_event(EVT_LE_META_EVENT, &nf);
	if ( setsockopt(device, SOL_HCI, HCI_FILTER, &nf, sizeof(nf)) < 0 ) 
	{
		hci_close_dev(device);
		perror("Could not set socket options\n");
		return 0;
	}

	printf("Scanning....\n");

	uint8_t buf[HCI_MAX_EVENT_SIZE];
	evt_le_meta_event * meta_event;
	le_advertising_info * info;
	int len;

	while ( 1 ) 
	{
		len = read(device, buf, sizeof(buf));
		
		if ( len >= HCI_EVENT_HDR_SIZE ) 
		{
			uint8_t ptype = buf[0];
			uint8_t event = buf[1];
			uint8_t plen = buf[2];
			uint16_t major = 0;
			uint16_t minor = 0;
			uint8_t counter = 0;
			int8_t string_start = 0;
			int8_t string_end = 0;
		
			meta_event = (evt_le_meta_event*)(buf+HCI_EVENT_HDR_SIZE+1);			// Populate our meta_event struct with data
			if ( meta_event->subevent == EVT_LE_ADVERTISING_REPORT ) 				// If we have an LE Advertising Report Packet
			{
				uint8_t reports_count = meta_event->data[0];						// Grab how many repoorts we need to process in this packet
				void * offset = meta_event->data + 1;
				while ( reports_count-- ) 
				{
					info = (le_advertising_info *)offset;
					char addr[18];
					ba2str(&(info->bdaddr), addr);
	
					// Debug Code - Display the Received Packet
					//printf("Packet: ");
					//while (counter <= plen) 										// Read the received packet in to our buffer
					//{
					//	printf ("\033[43;39m %02x \033[49m ", buf[counter]);		// Print out Hex Digit to the console
					//	counter ++;													// Move to the next byte to process
					//}
					//printf("\n");

					// Messy, need to clean up
					string_end = plen - 3;											// UUID Ends 3 Bytes before the last Byte in our packet
					if (string_end >= 16)											// If there are more than 20 Bytes to read
						string_start = string_end - 16;								// Then set our Start Byte for the UUID to 16 Bytes before the end
					else
						string_start = 4;											// Set our start byte to 4 Bytes in to the Packet
				
					counter = string_start;
				
					printf("UUID: ");
					while (counter < string_end) 									// Read the received packet in to our buffer
					{
						//uuid[(counter - string_start)] = buf[counter];			// Copy the byte to our UUID String

						printf ("\033[44;39m %02x \033[49m ", buf[counter]);		// Print out Hex Digit to the console
						counter ++;													// Move to the next byte to process
					}
					printf("\n");


					// Get the Device Major and Minor Values
					major = (buf[(plen - 3)] << 8);									// Get the MSB of the Major Value (4th Last Byte)
					major += (buf[(plen - 2)] & 255);								// Get the LSB of the Major Value (3rd Last Byte)

					minor = (buf[(plen - 1)] << 8);									// Get the MSB of the Minor Value (2nd Last Byte)
					minor += (buf[(plen)] & 255);									// Get the LSB of the Minor Value (Last Byte)
	
					//printf("%s - RSSI %d\n", addr, (char)info->data[info->length]);
					printf("MAC %s :: Major %i :: Minor %i :: TX Power %d :: RSSI %d\n", addr, major, minor, (int8_t)info->data[(info->length - 1)], (int8_t)info->data[info->length]);
					offset = info->data + info->length + 2;
				}
			}
		}
	}

	// Disable scanning.

	memset(&scan_cp, 0, sizeof(scan_cp));
	scan_cp.enable = 0x00;	// Disable flag.

	struct hci_request disable_adv_rq = ble_hci_request(OCF_LE_SET_SCAN_ENABLE, LE_SET_SCAN_ENABLE_CP_SIZE, &status, &scan_cp);
	ret = hci_send_req(device, &disable_adv_rq, 1000);
	if ( ret < 0 ) 
	{
		hci_close_dev(device);
		perror("Failed to disable scan.");
		return 0;
	}

	hci_close_dev(device);
	
	return 0;
}
