/*********************************************************************
* Software License Agreement (BSD License)
*
*  Copyright (c) 2009, 2010 Willow Garage, Inc.
*  All rights reserved.
*
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions
*  are met:
*
*   * Redistributions of source code must retain the above copyright
*     notice, this list of conditions and the following disclaimer.
*   * Redistributions in binary form must reproduce the above
*     copyright notice, this list of conditions and the following
*     disclaimer in the documentation and/or other materials provided
*     with the distribution.
*   * Neither the name of the Willow Garage nor the names of its
*     contributors may be used to endorse or promote products derived
*     from this software without specific prior written permission.
*
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
*  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
*  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
*  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
*  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
*  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
*  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
*  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
*  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
*  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
*  POSSIBILITY OF SUCH DAMAGE.
*********************************************************************/

/*
 * @file list.c
 *
 */
#include <stdlib.h>
#include "wge100_camera/list.h"
#include "wge100_camera/host_netutil.h"

/**
 * Initializes a new camera list using macros from list.h
 *
 * @param ipCamList Pointer to an IpCamList list element
 *
 * @return Always returns zero.
 */
int wge100CamListInit( IpCamList *ipCamList ) {
	INIT_LIST_HEAD(&ipCamList->list);
	return 0;
}


/**
* Adds a new camera to camera list ipCamList if that serial number is not
* already in the list.
*
* @param ipCamList 	Pointer to the IpCamList head
* @param newItem	Pointer to an IpCamList structure that describes the new camera
*
* @return Returns CAMLIST_ADD_OK if the camera was added to the list, or CAMLIST_ADD_DUP if it was a duplicate.
*/
int wge100CamListAdd( IpCamList *ipCamList, IpCamList *newItem) {
	IpCamList *listIterator;

	int isInList = 0;

	// Scan through the list, looking for a serial number that matches the one we're trying to add
	list_for_each_entry(listIterator, &(ipCamList->list), list) {
		if( newItem->serial == listIterator->serial) {
			// This camera is already in the list
			isInList = 1;
			break;
		}
	}

	if(isInList == 1) {
		wge100_debug("Serial number %d already exists in list, dropping.\n", newItem->serial);
		return CAMLIST_ADD_DUP;
	} else {
		wge100_debug("Serial number %d is new, adding to list.\n", newItem->serial);
		list_add_tail( &(newItem->list), &(ipCamList->list) );
		return CAMLIST_ADD_OK;
	}

}

/**
 * Utility function to locate a camera with a particular serial number in a list.
 * @param ipCamList	Pointer to the list head
 * @param serial	Serial number (not including product ID) to look for
 * @return			Returns the index of the camera, or -1 if not found
 */
int wge100CamListFind( IpCamList *ipCamList, uint32_t serial ) {
	int count;

	IpCamList *listIterator;
	count = 0;

	list_for_each_entry(listIterator, &(ipCamList->list), list) {
		if(listIterator->serial == serial) {
			return count;
		}
		count++;
	}

	return -1;
}

/**
 * Utility function to return a specific element number from the camera list.
 *
 * @pre index must be less than the value returned by wge100CamListNumEntries().
 * If it is greater, then the last element in the list will be returned.
 *
 * @param ipCamList 	Pointer to the camera list head
 * @param index		Number of the list element to returna (0..max)
 *
 * @return Returns a pointer to the requested list element
 */
IpCamList *wge100CamListGetEntry( const IpCamList *ipCamList, int index ) {
	IpCamList *listIterator;

	// Iterate through the list until we have counted off 'index' entries
	list_for_each_entry(listIterator, &(ipCamList->list), list) {
		if(index-- == 0) {
			break;
		}
	}

	return listIterator;
}

/**
 * Utility function to remove a specific element number from the camera list.
 *
 * @pre index must be less than the value returned by wge100CamListNumEntries().
 *
 * @param ipCamList 	Pointer to the camera list head
 * @param index		Number of the list element to remove (0..max)
 *
 * @return Returns 0 if successful, -1 if index was invalid
 */
int wge100CamListDelEntry( IpCamList *ipCamList, int index ) {
	int count;

	IpCamList *tmpListItem;
	struct list_head *pos, *q;
	count = 0;

	list_for_each_safe(pos, q,&(ipCamList->list)) {
		if(count++ == index) {
			tmpListItem = list_entry(pos, IpCamList, list);
			list_del(pos);
			free(tmpListItem);
			return 0;
		}
	}
	return -1;
}

/**
 * Utility function to determine the number of entries in an IpCamlist
 *
 * @param ipCamList	Pointer to the list head to count.
 *
 * @return Returns the number of elements in the list
 */
int wge100CamListNumEntries( const IpCamList *ipCamList ) {
	int count;

	IpCamList *listIterator;
	count = 0;

	list_for_each_entry(listIterator, &(ipCamList->list), list) {
		count++;
	}

	return count;
}

/**
 * Utility function to remove all entries from a camera list.
 *
 * @param ipCamList 	Pointer to the camera list head
 *
 * @return Returns 0 if successful.
 */
void wge100CamListDelAll( IpCamList *ipCamList ) {
	int count;

	IpCamList *tmpListItem;
	struct list_head *pos, *q;
	count = 0;

	list_for_each_safe(pos, q,&(ipCamList->list)) {
    tmpListItem = list_entry(pos, IpCamList, list);
    list_del(pos);
    free(tmpListItem);
	}
}
