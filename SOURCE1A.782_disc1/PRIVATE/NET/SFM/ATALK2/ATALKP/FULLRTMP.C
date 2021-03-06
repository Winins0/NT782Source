/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    fullrtmp.c

Abstract:

     Handle the RTMP protocol for a routing node.

Author:

    Garth Conboy     (Pacer Software)
    Nikhil Kamkolkar (NikhilK)

Revision History:

     GC - (03/13/91): Added the "recentRoutes" cache to improve large network
                      routing performance.
     GC - (01/20/92): Removed usage of numberOfConfiguredPorts; portDescriptors
                      may now be sparse, we use the portActive flag instead.
     GC - (01/21/92): Made RemoveFromRoutingTable externally visible.
     GC - (03/31/92): Updated for BufferDescriptors.
     GC - (06/15/92): Correct "notify neighbor" handling: don't allow route
                      upgrading.

--*/


#define IncludeFullRtmpErrors 1
#include "atalk.h"

#if IamNot an AppleTalkRouter
  // Empty file
#else

ExternForVisibleFunction TimerHandler RtmpSendTimerExpired;

ExternForVisibleFunction TimerHandler RtmpValidityTimerExpired;

ExternForVisibleFunction Boolean
        GetOrSetNetworkNumber(int port,
                              unsigned int suggestedNetworkNumber);

ExternForVisibleFunction void SendRoutingData(int port,
                                              AppleTalkAddress destination,
                                              Boolean doSplitHorizon);

ExternForVisibleFunction void
     EnterIntoRoutingTable(AppleTalkNetworkRange networkRange,
                           int port,
                           ExtendedAppleTalkNodeNumber nextRouter,
                           int numberOfHops);

static Boolean firstCall = True;




Boolean far StartRtmpProcessingOnPort(int port,
                                      ExtendedAppleTalkNodeNumber routerNode)
{
	
	  //
	  // For extended ports there is very little to do now; the process of getting
	  //   the node has done most of our work for us.
	  //
	
	  if (GET_PORTDESCRIPTOR(port)->ExtendedNetwork) {
		 //
		 // If we've seen another router; does the real network agree with what
		 //   we were going to try to seed?  No seeding for half ports.
		 //
	
		 if (GET_PORTDESCRIPTOR(port)->PortType isnt NonAppleTalkHalfPort and
			 GET_PORTDESCRIPTOR(port)->SeenRouterRecently and
			 GET_PORTDESCRIPTOR(port)->InitialNetworkRange.FirstNetworkNumber isnt
				  UnknownNetworkNumber)
			if (GET_PORTDESCRIPTOR(port)->InitialNetworkRange.FirstNetworkNumber isnt
				GET_PORTDESCRIPTOR(port)->ThisCableRange.FirstNetworkNumber or
				GET_PORTDESCRIPTOR(port)->InitialNetworkRange.LastNetworkNumber isnt
				GET_PORTDESCRIPTOR(port)->ThisCableRange.LastNetworkNumber)
			   ErrorLog("StartRtmpProcessingOnPort", ISevWarning, __LINE__, port,
						IErrFullRtmpIgnoredNetRange, IMsgFullRtmpIgnoredNetRange,
						Insert0());
	
		 // Otherwise, we're the first rotuer, so we'll seed (if we can).
	
		 if (GET_PORTDESCRIPTOR(port)->PortType is NonAppleTalkHalfPort) {
			GET_PORTDESCRIPTOR(port)->ThisCableRange.FirstNetworkNumber =
				  UnknownNetworkNumber;
			GET_PORTDESCRIPTOR(port)->ThisCableRange.LastNetworkNumber =
				  UnknownNetworkNumber;
		 }
		 else if (not GET_PORTDESCRIPTOR(port)->SeedRouter) {
			ErrorLog("StartRtmpProcessingOnPort", ISevError, __LINE__, port,
					 IErrFullRtmpNoSeedCantStart, IMsgFullRtmpNoSeedCantStart,
					 Insert0());
			return(False);
		 }
		 else if (not GET_PORTDESCRIPTOR(port)->SeenRouterRecently)
			GET_PORTDESCRIPTOR(port)->ThisCableRange =
					   GET_PORTDESCRIPTOR(port)->InitialNetworkRange;
	  }
	  else {
		 //
		 // For non-extended networks, we need to either find or seed our network
		 //   number.
		 //
	
		 if (not GetOrSetNetworkNumber(port, (unsigned int)(
										 GET_PORTDESCRIPTOR(port)->SeedRouter ?
										 GET_PORTDESCRIPTOR(port)->InitialNetworkRange.
											 FirstNetworkNumber :
										 UnknownNetworkNumber)))
			return(False);
	
		 if (not GET_PORTDESCRIPTOR(port)->SeenRouterRecently)
			GET_PORTDESCRIPTOR(port)->ThisCableRange.FirstNetworkNumber =
					 GET_PORTDESCRIPTOR(port)->InitialNetworkRange.FirstNetworkNumber;
	
		 //
		 // On non-extended networks, we would have "allocated" a node with network
		 //   zero, fix this up here!
		 //
	
		 GET_PORTDESCRIPTOR(port)->ActiveNodes->ExtendedNode.NetworkNumber =
			 GET_PORTDESCRIPTOR(port)->ARouter.NetworkNumber =
				  routerNode.NetworkNumber =
					   GET_PORTDESCRIPTOR(port)->ThisCableRange.FirstNetworkNumber;
	  }
	
	  // Well, we're a router now, so mark us as A-ROUTER.
	
	  GET_PORTDESCRIPTOR(port)->SeenRouterRecently = True;
	  GET_PORTDESCRIPTOR(port)->ARouter = routerNode;
	  GET_PORTDESCRIPTOR(port)->RouterRunning = True;
	
	  // Okay, make it so...
	
	  if (GET_PORTDESCRIPTOR(port)->PortType isnt NonAppleTalkHalfPort)
		 EnterIntoRoutingTable(GET_PORTDESCRIPTOR(port)->ThisCableRange, port,
							   routerNode, 0);
	
	  // Switch the incoming RTMP packet handler to be the router version.
	
	  CloseSocketOnNodeIfOpen(port, routerNode, RtmpSocket);
	  if (OpenSocketOnNode(empty, port, &routerNode, RtmpSocket, RtmpPacketInRouter,
						   (long)0, False, empty, 0, empty) isnt ATnoError) {
		 ErrorLog("StartRtmpProcessingOnPort", ISevError, __LINE__, port,
				  IErrFullRtmpBadSocketOpen, IMsgFullRtmpBadSocketOpen,
				  Insert0());
		 return(False);
	  }
	
	  // Start the two RTMP timers.
	
	  if (firstCall) {
		 StartTimer(RtmpSendTimerExpired, RtmpSendTimerSeconds, 0, empty);
		 StartTimer(RtmpValidityTimerExpired, RtmpValidityTimerSeconds, 0, empty);
		 firstCall = False;
	  }
	
	  return(True);
	
}  // StartRtmpProcessingOnPort




void far ShutdownFullRtmp(void)
{
	
	  firstCall = True;
	
}  // ShutdownFullRtmp




long far RtmpPacketInRouter(AppleTalkErrorCode errorCode,
                            long unsigned userData,
                            int port,
                            AppleTalkAddress source,
                            long destinationSocket,
                            int protocolType,
                            char far *datagram,
                            int datagramLength,
                            AppleTalkAddress actualDestination)
{
	AppleTalkNetworkRange cableRange;
	int rtmpCommand, responseSize, numberOfHops, index, indexToo;
	RoutingTableEntry routingTableEntry;
	ExtendedAppleTalkNodeNumber nextRouter;
	Boolean foundOverlap;
	BufferDescriptor buffer;
	
	// "Use" unneeded formals.
	
	actualDestination, userData;
	
	// Do we care?
	
	if (errorCode is ATsocketClosed)
	   return((long)True);
	else if (errorCode isnt ATnoError) {
		ErrorLog("RtmpPacketInRouter", ISevError, __LINE__, port,
				IErrFullRtmpBadIncomingError, IMsgFullRtmpBadIncomingError,
				Insert0());
		return((long)True);
	}
	
	// Is the packet long enough to have any interestng data?
	
	if (protocolType is DdpProtocolRtmpRequest) {
		if (datagramLength < RtmpRequestDatagramSize)
		{
		  ErrorLog("RtmpPacketInRouter", ISevWarning, __LINE__, port,
				   IErrFullRtmpReqTooShort, IMsgFullRtmpReqTooShort,
				   Insert0());
		  return((long)True);
		}
	}
	else if (protocolType is DdpProtocolRtmpResponseOrData) {
		if ((GET_PORTDESCRIPTOR(port)->ExtendedNetwork and
			datagramLength < RtmpDataMinimumSizeExtended) or
		   (not GET_PORTDESCRIPTOR(port)->ExtendedNetwork and
			datagramLength < RtmpDataMinimumSizeNonExtended)) {
		  ErrorLog("RtmpPacketInRouter", ISevWarning, __LINE__, port,
				   IErrFullRtmpDataTooShort, IMsgFullRtmpDataTooShort,
				   Insert0());
		  return((long)True);
		}
	}
	else
	   return((long)True);     // Funny protocol type...
	
	// We're going to be playing with the routing tables...
	
	DeferTimerChecking();
	DeferIncomingPackets();
	
	if (protocolType is DdpProtocolRtmpRequest) {
		
		   rtmpCommand = (unsigned char)datagram[RtmpRequestCommandOffset];
		   switch (rtmpCommand) {
			  case RtmpRequestCommand:
				 break;
		
			  case RtmpDataRequestCommand:
			  case RtmpEntireDataRequestCommand:
		
			   //
				 // Send all or part of our routing tables back to the requesting
				 //   address...
			   //
		
				 SendRoutingData(port, source,
								 (Boolean)(rtmpCommand is RtmpDataRequestCommand));
				 HandleIncomingPackets();
				 HandleDeferredTimerChecks();
				 return((long)True);
		
			  default:
				 ErrorLog("RtmpPacketInRouter", ISevWarning, __LINE__, port,
						  IErrFullRtmpBadReqCommand, IMsgFullRtmpBadReqCommand,
						  Insert0());
				 HandleIncomingPackets();
				 HandleDeferredTimerChecks();
				 return((long)True);
		   }
		
			//
		   // Okay, we're a standard RtmpRequest; do the right thing.  Find our
		   //   port's entry in the routing table.
			//
		
		   if ((routingTableEntry =
				FindInRoutingTable(GET_PORTDESCRIPTOR(port)->ARouter.
								   networkNumber)) is empty) {
			  ErrorLog("RtmpPacketInRouter", ISevError, __LINE__, port,
					   IErrFullRtmpBadRoutingTables, IMsgFullRtmpBadRoutingTables,
					   Insert0());
			  HandleIncomingPackets();
			  HandleDeferredTimerChecks();
			  return((long)True);
		   }
		
		   // This guy would like an RTMP response...
		
		   if ((buffer = NewBufferDescriptor(RtmpResponseMaxSize)) is Empty) {
			  ErrorLog("RtmpPacketInRouter", ISevError, __LINE__, port,
					   IErrFullRtmpOutOfMemory, IMsgFullRtmpOutOfMemory,
					   Insert0());
			  HandleIncomingPackets();
			  HandleDeferredTimerChecks();
			  return((long)True);
		   }
		
		   buffer->data[RtmpSendersNetworkOffset] =
				(char)((GET_PORTDESCRIPTOR(port)->ARouter.NetworkNumber >> 8) & 0xFF);
		   buffer->data[RtmpSendersNetworkOffset + 1] =
				(char)(GET_PORTDESCRIPTOR(port)->ARouter.NetworkNumber & 0xFF);
		   buffer->data[RtmpSendersIdLengthOffset] = 8;  // Bits
		   buffer->data[RtmpSendersIdOffset] = GET_PORTDESCRIPTOR(port)->ARouter.NodeNumber;
		
			//
		   // On an extended port, we also want to add the initial network range
		   //   tuple.
			//
		
		   if (not GET_PORTDESCRIPTOR(port)->ExtendedNetwork)
			  responseSize = RtmpSendersIdOffset + sizeof(char);
		   else {
			  buffer->data[RtmpRangeStartOffset] =
					(char)((GET_PORTDESCRIPTOR(port)->ThisCableRange.
							FirstNetworkNumber >> 8) & 0xFF);
			  buffer->data[RtmpRangeStartOffset + 1] =
					(char)(GET_PORTDESCRIPTOR(port)->ThisCableRange.
						   FirstNetworkNumber & 0xFF);
			  buffer->data[RtmpRangeStartOffset + 2] = RtmpTupleWithRange;
			  buffer->data[RtmpRangeEndOffset] =
					(char)((GET_PORTDESCRIPTOR(port)->ThisCableRange.
							LastNetworkNumber >> 8) & 0xFF);
			  buffer->data[RtmpRangeEndOffset + 1] =
					(char)(GET_PORTDESCRIPTOR(port)->ThisCableRange.
						   LastNetworkNumber & 0xFF);
			  responseSize = RtmpRangeEndOffset + sizeof(short unsigned);
		   }
		
		   // Send the response.
		
		   if (DeliverDdp(destinationSocket, source, DdpProtocolRtmpResponseOrData,
						  buffer, responseSize, Empty, Empty, 0) isnt ATnoError)
			  ErrorLog("RtmpPacketInRouter", ISevError, __LINE__, port,
					   IErrFullRtmpBadRespSend, IMsgFullRtmpBadRespSend,
					   Insert0());
		
	}
	else if (protocolType is DdpProtocolRtmpResponseOrData) {
		if ((unsigned char)datagram[RtmpSendersIdLengthOffset] isnt 8)
		{
		  ErrorLog("RtmpPacketInRouter", ISevWarning, __LINE__, port,
				   IErrFullRtmpBadNodeIdLen, IMsgFullRtmpBadNodeIdLen,
				   Insert0());
		  HandleIncomingPackets();
		  HandleDeferredTimerChecks();
		  return((long)True);
		}
	
		// For non extended networks, we should have a leading version stamp.
	
		if (GET_PORTDESCRIPTOR(port)->ExtendedNetwork)
		  index = RtmpSendersIdOffset + 1;
		else
		  if ((unsigned char)datagram[RtmpSendersIdOffset + 1] isnt 0 or
			  (unsigned char)datagram[RtmpSendersIdOffset + 2] isnt 0 or
			  (unsigned char)datagram[RtmpSendersIdOffset + 3] isnt
				RtmpVersionNumber) {
			 ErrorLog("RtmpPacketInRouter", ISevError, __LINE__, port,
					  IErrFullRtmpBadVersion, IMsgFullRtmpBadVersion,
					  Insert0());
			 HandleIncomingPackets();
			 HandleDeferredTimerChecks();
			 return((long)True);
		  }
		  else
			 index = RtmpSendersIdOffset + 4;
	
		//
		// Walk through the routing tuples... The "+ 3" ensure we as least have
		//   a non-extended tuple.
		//
	
		while (index + 3 <= datagramLength) {
		  // Decode either a short or long tuple...
	
		  cableRange.firstNetworkNumber = (unsigned short)
						  (((unsigned char)datagram[index++]) << 8);
		  cableRange.firstNetworkNumber += (unsigned char)datagram[index++];
		  numberOfHops = (unsigned char)datagram[index++];
		  if (numberOfHops & RtmpExtendedTupleMask) {
			 if (index + 2 > datagramLength)
			 {
				ErrorLog("RtmpPacketInRouter", ISevError, __LINE__, port,
						 IErrFullRtmpBadTuple, IMsgFullRtmpBadTuple,
						 Insert0());
				HandleIncomingPackets();
				HandleDeferredTimerChecks();
				return((long)True);
			 }
			 cableRange.lastNetworkNumber = (unsigned short)
							 (((unsigned char)datagram[index++]) << 8);
			 cableRange.lastNetworkNumber += (unsigned char)datagram[index++];
			 if ((unsigned char)datagram[index++] isnt RtmpVersionNumber) {
				ErrorLog("RtmpPacketInRouter", ISevWarning, __LINE__, port,
						 IErrFullRtmpBadVersionExt, IMsgFullRtmpBadVersionExt,
						 Insert0());
				HandleIncomingPackets();
				HandleDeferredTimerChecks();
				return((long)True);
			 }
		  }
		  else
			 cableRange.lastNetworkNumber = cableRange.firstNetworkNumber;
		  numberOfHops &= RtmpNumberOfHopsMask;
	
		//
		  // Skip dummy "this net" on half ports; the first tuple coming in
		  //   from a half port will fall into this catagory (having no network
		  //   range).
		//
	
		  if (GET_PORTDESCRIPTOR(port)->PortType is NonAppleTalkHalfPort and
			  cableRange.firstNetworkNumber is UnknownNetworkNumber)
			 continue;
	
		  // Validate what we have...
	
		  if (not CheckNetworkRange(cableRange))
			 continue;
	
		//
		  // Okay, first check to see if this tuple concerns a network range
		  //   that we already know about.
		//
	
		  if ((routingTableEntry =
			   FindInRoutingTable(cableRange.firstNetworkNumber)) isnt empty and
			  routingTableEntry->networkRange.firstNetworkNumber is
					 cableRange.firstNetworkNumber and
			  routingTableEntry->networkRange.lastNetworkNumber is
					 cableRange.lastNetworkNumber) {
			 // Check for "notify neighbor" telling us that an entry is bad.
		
			 if (numberOfHops is 31 and
				 routingTableEntry->nextRouter.networkNumber is
					 source.networkNumber and
				 routingTableEntry->nextRouter.nodeNumber is source.nodeNumber) {
				// Don't "upgrade" route to PrettyBad from Bad...
		
				if (routingTableEntry->entryState isnt Bad)
				   routingTableEntry->entryState = PrettyBad;
				continue;
			 }
		
		   //
			 // If we're hearing about one of our directly connected nets, we
			 //   know best... ignore the info.
		   //
		
			 if (routingTableEntry->numberOfHops is 0)
				continue;
		
		   //
			 // Check for previously bad entry, and a short enough path
			 //   with this tuple... if so, replace the entry.
		   //
		
			 if ((routingTableEntry->entryState is PrettyBad or
				  routingTableEntry->entryState is Bad) and
				 numberOfHops < 15) {
				routingTableEntry->numberOfHops = (short)(numberOfHops + 1);
				routingTableEntry->nextRouter.networkNumber =
						  source.networkNumber;
				routingTableEntry->nextRouter.nodeNumber =
						  source.nodeNumber;
				routingTableEntry->port = (short)port;
				routingTableEntry->entryState = Good;
				continue;
			 }
		
		   //
			 // Check fora shorter or equidistant path to the target network...
			 //   if so, replace the entry.
		   //
		
			 if (routingTableEntry->numberOfHops >= numberOfHops + 1 and
				 numberOfHops < 15) {
				routingTableEntry->numberOfHops = (short)(numberOfHops + 1);
				routingTableEntry->nextRouter.networkNumber =
						  source.networkNumber;
				routingTableEntry->nextRouter.nodeNumber =
						  source.nodeNumber;
				routingTableEntry->port = (short)port;
				routingTableEntry->entryState = Good;
				continue;
			 }
		
		   //
			 // Check for the same router still thinking it has a path to the
			 //   network, but it's further away now... if so, update the entry.
		   //
		
			 if (routingTableEntry->nextRouter.networkNumber is
						  source.networkNumber and
				 routingTableEntry->nextRouter.nodeNumber is
						  source.nodeNumber and
				 routingTableEntry->port is (short)port) {
				routingTableEntry->numberOfHops = (short)(numberOfHops + 1);
				if (routingTableEntry->numberOfHops < 16)
				   routingTableEntry->entryState = Good;
				else
				   RemoveFromRoutingTable(cableRange);
				continue;
			 }
			 continue;
		  }
	
		//
		  // Otherwise, we need to walk through the entire routing table
		  //   making sure the current tuple doesn't overlap with anything
		  //   we already have (since it didn't exactly match) -- if we find
		  //   an overlap, ignore the tuple (a network configuration error,
		  //   no doubt), else add it as a new network range!
		//
	
		  for (foundOverlap = False, indexToo = 0;
			   not foundOverlap and indexToo < NumberOfRtmpHashBuckets;
			   indexToo += 1)
			 for (routingTableEntry = routingTable[indexToo];
				  not foundOverlap and routingTableEntry isnt empty;
				  routingTableEntry = routingTableEntry->next)
				if (RangesOverlap(&cableRange, &routingTableEntry->networkRange))
				   foundOverlap = True;
	
		  if (foundOverlap) {
			 ErrorLog("RtmpPacketInRouter", ISevWarning, __LINE__, port,
					  IErrFullRtmpOverlappingNets, IMsgFullRtmpOverlappingNets,
					  Insert0());
			 continue;
		  }
	
		  // Okay, enter this new network range!
	
		  nextRouter.networkNumber = source.networkNumber;
		  nextRouter.nodeNumber = source.nodeNumber;
		  if (numberOfHops < 15)
			 EnterIntoRoutingTable(cableRange, port, nextRouter,
								   numberOfHops + 1);
	
		}   // Walk through all of the tuples loop.
	
	}  // Protocol type is DdpProtocolRtmpResponseOrData
	
	HandleIncomingPackets();
	HandleDeferredTimerChecks();
	return((long)True);
	
}  // RtmpPacketInRouter




RoutingTableEntry far FindInRoutingTable(short unsigned networkNumber)
{
	int index, recentRouteIndex;
	RoutingTableEntry routingTableEntry;
	
	// First, try the "recent route cache".
	
	CheckMod(recentRouteIndex, networkNumber, NumberOfRecentRouteBuckets,
			 "FindInRoutingTable");
	if (recentRoutes[recentRouteIndex] isnt empty and
		IsWithinNetworkRange(networkNumber,
							 &recentRoutes[recentRouteIndex]->networkRange))
	   return(recentRoutes[recentRouteIndex]);
	
	//
	// Find a given network number in the RTMP routing table.  We maintain the
	//   routing table as a hashed lookup structure based upon the first network
	//   of the given network range.  First, check to see if we're lucky and the
	//   target network number is the start of a range...
	//
	
	CheckMod(index, networkNumber, NumberOfRtmpHashBuckets,
			 "FindInRoutingTable");
	
	for (routingTableEntry = routingTable[index];
		 routingTableEntry isnt empty;
		 routingTableEntry = routingTableEntry->next)
	   if (routingTableEntry->networkRange.firstNetworkNumber is networkNumber)
		  break;
	
	// Cache and return our find!
	
	if (routingTableEntry isnt empty) {
		recentRoutes[recentRouteIndex] = routingTableEntry;
		return(routingTableEntry);
	}
	
	//
	// Otherwise, we need to walk the entire routing table to see if this guy
	//   is within any range!  Sigh.  If found, cache the find.
	//
	
	for (index = 0; index < NumberOfRtmpHashBuckets; index += 1)
	   for (routingTableEntry = routingTable[index];
			routingTableEntry isnt empty;
			routingTableEntry = routingTableEntry->next)
		  if (IsWithinNetworkRange(networkNumber,
								   &routingTableEntry->networkRange)) {
			 recentRoutes[recentRouteIndex] = routingTableEntry;
			 return(routingTableEntry);
		  }
	
	// Oops, nobody home!
	
	return(empty);
	
}  // FindInRoutingTable




Boolean far RemoveFromRoutingTable(AppleTalkNetworkRange networkRange)
{
	int index;
	RoutingTableEntry routingTableEntry, previousRoutingTableEntry = empty;
	
	//
	// Remove a given network number from the RTMP routing table.  Return True
	//   if we can find/remove the network; False otherwise.
	//
	
	CheckMod(index, networkRange.firstNetworkNumber, NumberOfRtmpHashBuckets,
			 "RemoveFromRoutingTable");
	
	for (routingTableEntry = routingTable[index];
		 routingTableEntry isnt empty;
		 routingTableEntry = routingTableEntry->next)
	   if (routingTableEntry->networkRange.firstNetworkNumber is
				networkRange.firstNetworkNumber and
		   routingTableEntry->networkRange.lastNetworkNumber is
				networkRange.lastNetworkNumber) {
		  // Unthread the guy who's leaving...
	
		  if (previousRoutingTableEntry is empty)
			 routingTable[index] = routingTableEntry->next;
		  else
			 previousRoutingTableEntry->next = routingTableEntry->next;
	
		//
		  // Do a pass through the "recent router cache", remove this guy if
		  //   he's there.
		//
	
		  for (index = 0; index < NumberOfRecentRouteBuckets; index += 1)
			 if (recentRoutes[index] is routingTableEntry) {
				recentRoutes[index] = empty;
				break;
			 }
	
		  // Free the zone list and the actual routing table node.
	
		  FreeZoneList(routingTableEntry->zoneList);
		  Free(routingTableEntry);
	
		  // The deed is done.
	
		  return(True);
	   }
	   else
		  previousRoutingTableEntry = routingTableEntry;
	
	return(False);
	
}  // RemoveFromRoutingTable




ExternForVisibleFunction Boolean
        GetOrSetNetworkNumber(int port,
                              unsigned int suggestedNetworkNumber)
{
	// We use this beast for non-extended networks only!
	
	//
	// The plan here is not to mess up a working Internet.  So,
	//   we'll send out a few RTMP request packets, if we can find the network
	//   number of the network, we'll use that one and ignore the one that was
	//   passed in.  Otherwise, we'll use the one that was passed in, unless
	//   that is zero, in which case we'll blow the user away.  A zero value for
	//   our argument indicates that the caller does not want to set the network
	//   network-number and we should expect to find at least one bridge already
	//   operating out there on the specified port.
	//
	
	BufferDescriptor buffer;
	AppleTalkAddress source, destination;
	int numberOfRequests = 0;
	
	if (GET_PORTDESCRIPTOR(port)->ExtendedNetwork) {
		ErrorLog("GetOrSetNetworkNumber", ISevError, __LINE__, port,
				IErrFullRtmpSigh, IMsgFullRtmpSigh,
				Insert0());
		return(False);
	}
	
	// Set up the source, destination, and our very simple request packet.
	
	source.networkNumber = destination.networkNumber = UnknownNetworkNumber;
	source.nodeNumber =
		   GET_PORTDESCRIPTOR(port)->ActiveNodes->ExtendedNode.NodeNumber;
	destination.nodeNumber = AppleTalkBroadcastNodeNumber;
	source.socketNumber = destination.socketNumber = RtmpSocket;
	
	//
	// Blast a number of requests out on the wire, see if anybody will let us in
	//   on the network network-number.
	//
	
	while (not GET_PORTDESCRIPTOR(port)->SeenRouterRecently and
		   numberOfRequests < NumberOfRtmpRequests) {
		// Build the request packet.
	
		if ((buffer = NewBufferDescriptor(RtmpRequestDatagramSize)) is Empty) {
		  ErrorLog("GetOrSetNetworkNumber", ISevError, __LINE__, port,
				   IErrFullRtmpOutOfMemory, IMsgFullRtmpOutOfMemory,
				   Insert0());
		  return(False);
		}
	
		buffer->data[RtmpRequestCommandOffset] = RtmpRequestCommand;
	
		if (not TransmitDdp(port, source, destination, DdpProtocolRtmpRequest,
						   buffer, RtmpRequestDatagramSize, 0, empty, empty,
						   Empty, 0)) {
		  ErrorLog("GetOrSetNetworkNumber", ISevError, __LINE__, port,
				   IErrFullRtmpBadReqSend, IMsgFullRtmpBadReqSend,
				   Insert0());
		  return(False);
		}
		numberOfRequests += 1;
		WaitFor(RtmpRequestTimerInHundreths,
			   &GET_PORTDESCRIPTOR(port)->SeenRouterRecently);
	}
	
	// If we got an answer, we're set...
	
	if (GET_PORTDESCRIPTOR(port)->SeenRouterRecently) {
		if (suggestedNetworkNumber isnt UnknownNetworkNumber and
		   GET_PORTDESCRIPTOR(port)->ThisCableRange.FirstNetworkNumber isnt
				(short unsigned)suggestedNetworkNumber)
		  ErrorLog("GetOrSetNetworkNumber", ISevWarning, __LINE__, port,
				   IErrFullRtmpIgnoredNet, IMsgFullRtmpIgnoredNet,
				   Insert0());
		return(True);
	}
	
	//
	// If we didn't get an answer, we had better have a suggested network number
	//   passed in.
	//
	
	if (suggestedNetworkNumber is UnknownNetworkNumber) {
		ErrorLog("GetOrSetNetworkNumber", ISevError, __LINE__, port,
				IErrFullRtmpNoSeedCantStart, IMsgFullRtmpNoSeedCantStart,
				Insert0());
		return(False);
	}
	
	GET_PORTDESCRIPTOR(port)->ThisCableRange.FirstNetworkNumber =
				(short unsigned)suggestedNetworkNumber;
	return(True);
	
}  // GetOrSetNetworkNumber




ExternForVisibleFunction void
     EnterIntoRoutingTable(AppleTalkNetworkRange networkRange,
                           int port,
                           ExtendedAppleTalkNodeNumber nextRouter,
                           int numberOfHops)
{
	RoutingTableEntry routingTableEntry;
	int index;
	
	//
	// Make a new entry in our RTMP routing table.  We assume that we are passed
	//   a legitimate new entry (i.e. all range overlaps have already been checked
	//   for).
	//
	
	CheckMod(index, networkRange.firstNetworkNumber, NumberOfRtmpHashBuckets,
			 "EnterIntoRoutingTable");
	
	if (networkRange.lastNetworkNumber is UnknownNetworkNumber)
	   networkRange.lastNetworkNumber = networkRange.firstNetworkNumber;
	
	routingTableEntry = (RoutingTableEntry)Calloc(sizeof(*routingTableEntry), 1);
	routingTableEntry->networkRange = networkRange;
	routingTableEntry->port = (short)port;
	routingTableEntry->nextRouter = nextRouter;
	routingTableEntry->numberOfHops = (short)numberOfHops;
	routingTableEntry->entryState = Good;
	
	// Thread the new entry into the head of the hash list.
	
	routingTableEntry->next = routingTable[index];
	routingTable[index] = routingTableEntry;
	
}  // EnterIntoRoutingTable




ExternForVisibleFunction void far RtmpSendTimerExpired(long unsigned timerId,
                                                       int additionalDataSize,
                                                       char far *additionalData)
{
	AppleTalkAddress destination;
	int port;
	
	// "Use" unneeded actual parameters.
	
	timerId, additionalDataSize, additionalData;
	
	// Broadcast out each port a split horizon view of our routing tables.
	
	destination.networkNumber = CableWideBroadcastNetworkNumber;
	destination.nodeNumber = AppleTalkBroadcastNodeNumber;
	destination.socketNumber = RtmpSocket;
	for (port = 0; port < MaximumNumberOfPorts; port += 1)
	   if (GET_PORTDESCRIPTOR(port)->Flags & PD_ACTIVE and
		   GET_PORTDESCRIPTOR(port)->RouterRunning) {
		  DeferIncomingPackets();
		  SendRoutingData(port, destination, True);
		  HandleIncomingPackets();
	   }
	
	// Re-arm the send RTMP timer...
	
	StartTimer(RtmpSendTimerExpired, RtmpSendTimerSeconds, 0, empty);
	
	// We've done the deed...
	
	HandleDeferredTimerChecks();
	return;
	
}  // RtmpSendTimerExpired




ExternForVisibleFunction void far
     RtmpValidityTimerExpired(long unsigned timerId,
                              int additionalDataSize,
                              char far *additionalData)
{
	RoutingTableEntry routingTableEntry, nextRoutingTableEntry;
	int index;
	
	// "Use" unneeded actual parameters.
	
	timerId, additionalDataSize, additionalData;
	
	//
	// We're going to muck with the routing databases, so defer incoming
	//   packets while we do the deed.
	//
	
	DeferIncomingPackets();
	
	// Walk the routing table aging the entries...
	
	for (index = 0; index < NumberOfRtmpHashBuckets; index += 1)
	   for (routingTableEntry = routingTable[index];
			routingTableEntry isnt empty;
			routingTableEntry = nextRoutingTableEntry) {
		  // Get the next entry first, we may delete the current entry...
	
		  nextRoutingTableEntry = routingTableEntry->next;
	
		  switch (routingTableEntry->entryState) {
			 case Good:
				if (routingTableEntry->numberOfHops isnt 0)
				   routingTableEntry->entryState = Suspect;
				break;
			 case Suspect:
				routingTableEntry->entryState = PrettyBad;
				break;
			 case PrettyBad:
				routingTableEntry->entryState = Bad;
				break;
			 case Bad:
				RemoveFromRoutingTable(routingTableEntry->networkRange);
				break;
			 default:
				ErrorLog("RtmpValidityTimerExpired", ISevError, __LINE__,
						 routingTableEntry->port,
						 IErrFullRtmpBadEntryState, IMsgFullRtmpBadEntryState,
						 Insert0());
				RemoveFromRoutingTable(routingTableEntry->networkRange);
				break;
		  }
	   }
	
	// Re-arm the Validity timer...
	
	StartTimer(RtmpValidityTimerExpired, RtmpValidityTimerSeconds, 0, empty);
	
	// We've done the deed...
	
	HandleIncomingPackets();
	HandleDeferredTimerChecks();
	return;
	
}  // RtmpValidityTimerExpired




ExternForVisibleFunction void SendRoutingData(int port,
                                              AppleTalkAddress destination,
                                              Boolean doSplitHorizon)
{
	RoutingTableEntry routingTableEntry;
	BufferDescriptor datagram;
	int index, hash, staticHeaderSize;
	Boolean newPacket = True, unsentTuples = False;
	AppleTalkAddress source;
	long sourceSocket;
	ExtendedAppleTalkNodeNumber sourceNode;
	
	// First, compute the source socket: Rtmp socket on our routers node.
	
	source.networkNumber = GET_PORTDESCRIPTOR(port)->ARouter.NetworkNumber;
	source.nodeNumber = GET_PORTDESCRIPTOR(port)->ARouter.NodeNumber;
	source.socketNumber = RtmpSocket;
	if ((sourceSocket = MapAddressToSocket(port, source)) < ATnoError) {
		//
	   // This may be okay during initialization... we're not started on all
	   //   ports yet.
		//
	
	   return;
	}
	
	//
	// Set up the static header to an RTMP data datagram (routers net/node
	//   number).  For half ports, since we really don't have such info fill in
	//   the address of the first non-half-port routing port.
	//
	
	if (GET_PORTDESCRIPTOR(port)->PortType isnt NonAppleTalkHalfPort)
	   sourceNode = GET_PORTDESCRIPTOR(port)->ARouter;
	else {
		for (index = 0; index < MaximumNumberOfPorts; index += 1)
		  if (GET_PORTDESCRIPTOR(index)->Flags & PD_ACTIVE and
			  GET_PORTDESCRIPTOR(index)->PortType isnt NonAppleTalkHalfPort and
			  GET_PORTDESCRIPTOR(index)->RouterRunning) {
			 sourceNode = GET_PORTDESCRIPTOR(index)->ARouter;
			 break;
		  }
	
		//
		// If we didn't find an address we like, just use the dummy one in
		//   our current port (no doubt net/node = 0).
		//
	
		if (index >= MaximumNumberOfPorts)
		  sourceNode = GET_PORTDESCRIPTOR(port)->ARouter;
	}
	
	// Get the first buffer descriptor.
	
	if ((datagram = NewBufferDescriptor(MaximumDdpDatagramSize)) is Empty) {
		ErrorLog("SendRoutingData", ISevError, __LINE__, port,
				IErrFullRtmpOutOfMemory, IMsgFullRtmpOutOfMemory,
				Insert0());
		return;
	}
	
	//
	// Walk through the routing table building a tuple for each network;
	//   we may have to send multiple packets...
	//
	
	for (hash = 0; hash < NumberOfRtmpHashBuckets; hash += 1)
	   for (routingTableEntry = routingTable[hash];
			routingTableEntry isnt empty;
			routingTableEntry = routingTableEntry->next) {
		  if (newPacket)
		  {
			 // Build the static part of an RtmpData packet.
	
			 MoveShortMachineToWire(datagram->data + RtmpSendersNetworkOffset,
									sourceNode.networkNumber);
			 datagram->data[RtmpSendersIdLengthOffset] = 8;   // Bits
			 datagram->data[RtmpSendersIdOffset] = sourceNode.nodeNumber;
	
		   //
			 // For non-extended networks we need the version stamp, for exteneded
			 //   networks, we need to include a initial network range tuple as
			 //   part of the header.
		   //
	
			 if (not GET_PORTDESCRIPTOR(port)->ExtendedNetwork) {
				datagram->data[RtmpSendersIdOffset + 1] = 0;
				datagram->data[RtmpSendersIdOffset + 2] = 0;
				datagram->data[RtmpSendersIdOffset + 3] = RtmpVersionNumber;
																   //
				staticHeaderSize = RtmpSendersIdOffset + 1 + 2 + 1;  // + nodeID +
																	 //   filler +
																	 //   version
																   //
			 }
			 else {
				datagram->data[RtmpRangeStartOffset] =
					  (char)((GET_PORTDESCRIPTOR(port)->ThisCableRange.
							  FirstNetworkNumber >> 8) & 0xFF);
				datagram->data[RtmpRangeStartOffset + 1] =
					  (char)(GET_PORTDESCRIPTOR(port)->ThisCableRange.
							 FirstNetworkNumber & 0xFF);
				datagram->data[RtmpRangeStartOffset + 2] = RtmpTupleWithRange;
				datagram->data[RtmpRangeEndOffset] =
					  (char)((GET_PORTDESCRIPTOR(port)->ThisCableRange.
							  LastNetworkNumber >> 8) & 0xFF);
				datagram->data[RtmpRangeEndOffset + 1] =
					  (char)(GET_PORTDESCRIPTOR(port)->ThisCableRange.
							 LastNetworkNumber & 0xFF);
				datagram->data[RtmpRangeEndOffset + 2] = RtmpVersionNumber;
															   //
				staticHeaderSize = RtmpRangeEndOffset + 2 + 1;   // + last net number +
																 //     version
															   //
				#if Verbose and (IamNot a WindowsNT)
				   printf("   RTMP Tuple: port %d; net \"%d:%d\"; distance %d.\n", port,
						  GET_PORTDESCRIPTOR(port)->ThisCableRange.FirstNetworkNumber,
						  GET_PORTDESCRIPTOR(port)->ThisCableRange.LastNetworkNumber,
						  0);
				#endif
			 }
	
		   //
			 // Start of a new packet full of tuples; set the packet index to
			 //   the start up the tuples.
		   //
	
			 index = staticHeaderSize;
			 newPacket = False;
		  }
	
		//
		  // Should the current tuple be omitted due to split horizon
		  //   processing?
		//
	
		  if (doSplitHorizon)
			 if (routingTableEntry->numberOfHops isnt 0)
				if (routingTableEntry->port is port)
				   continue;
	
		  // Okay, place the tuple in the packet...
	
		  datagram->data[index++] =
				(char)(routingTableEntry->networkRange.firstNetworkNumber >> 8);
		  datagram->data[index++] =
				(char)(routingTableEntry->networkRange.firstNetworkNumber & 0xFF);
	
		  // Do "notify nieghbor" if our current state is bad.
	
		  if (routingTableEntry->entryState is PrettyBad or
			  routingTableEntry->entryState is Bad)
			 datagram->data[index++] = RtmpNumberOfHopsMask;
		  else
			 datagram->data[index++] = (char)(routingTableEntry->numberOfHops &
											  RtmpNumberOfHopsMask);
	
		//
		  // Send an extended tuple if the network range isn't one or the target
		  //   port is an extended network.
		//
	
		  if (GET_PORTDESCRIPTOR(port)->ExtendedNetwork or
			  routingTableEntry->networkRange.firstNetworkNumber isnt
					 routingTableEntry->networkRange.lastNetworkNumber) {
			 datagram->data[index - 1] |= RtmpExtendedTupleMask;
			 datagram->data[index++] =
				(char)(routingTableEntry->networkRange.lastNetworkNumber >> 8);
			 datagram->data[index++] =
				(char)(routingTableEntry->networkRange.lastNetworkNumber & 0xFF);
			 datagram->data[index++] = RtmpVersionNumber;
		  }
	
		  #if Verbose and (IamNot a WindowsNT)
			 printf("   RTMP Tuple: port %d; net \"%d:%d\"; distance %d.\n", port,
					routingTableEntry->networkRange.firstNetworkNumber,
					routingTableEntry->networkRange.lastNetworkNumber,
					routingTableEntry->numberOfHops);
		  #endif
	
		  unsentTuples = True;
	
		  // Send a data packet if we're full...
	
		  if (index + RtmpExtendedTupleSize >= MaximumDdpDatagramSize) {
			 // Send the DDP packet...
	
			 if (DeliverDdp(sourceSocket, destination,
							DdpProtocolRtmpResponseOrData,
							datagram, index, Empty, Empty, 0) isnt ATnoError)
				ErrorLog("SendRoutingData", ISevError, __LINE__, port,
						 IErrFullRtmpBadDataSend, IMsgFullRtmpBadDataSend,
						 Insert0());
	
			 // Get the next buffer descriptor.
	
			 if ((datagram = NewBufferDescriptor(MaximumDdpDatagramSize)) is
				 Empty) {
				ErrorLog("SendRoutingData", ISevError, __LINE__, port,
						 IErrFullRtmpOutOfMemory, IMsgFullRtmpOutOfMemory,
						 Insert0());
				return;
			 }
	
			 newPacket = True;
			 unsentTuples = False;
		  }
	   }
	
	//
	// If "unsentTuples" is set then we have a partial packet that we need
	//   to send.
	//
	
	if (unsentTuples) {
		if (DeliverDdp(sourceSocket, destination,
					  DdpProtocolRtmpResponseOrData,
					  datagram, index, Empty, Empty, 0) isnt ATnoError)
		  ErrorLog("SendRoutingData", ISevError, __LINE__, port,
				   IErrFullRtmpBadDataSend, IMsgFullRtmpBadDataSend,
				   Insert0());
	}
	else
	   FreeBufferChain(datagram);   // Free unused buffer chain.
	
	// The deed is done.
	
	return;
	
}  // SendRoutingData




#if (Verbose and (IamNot a WindowsNT)) or (Iam a Primos)
void DumpRtmpRoutingTable(void)
{
	RoutingTableEntry routingTableEntry;
	int index;
	ZoneList zone;
	char *p;
	
	DeferTimerChecking();
	DeferIncomingPackets();
	
	// Dump the RTMP routing tables for debugging.
	
	printf("********** RTMP routing table start:\n");
	for (index = 0; index < NumberOfRtmpHashBuckets; index += 1)
	   for (routingTableEntry = routingTable[index];
			routingTableEntry isnt empty;
			routingTableEntry = routingTableEntry->next) {
		  printf("Range %05u:%05u; port %d; state %d; hops %02d; "
				 "next bridge %u.%u.\n",
				 routingTableEntry->networkRange.firstNetworkNumber,
				 routingTableEntry->networkRange.lastNetworkNumber,
				 routingTableEntry->port,
				 routingTableEntry->entryState,
				 routingTableEntry->numberOfHops,
				 routingTableEntry->nextRouter.networkNumber,
				 routingTableEntry->nextRouter.nodeNumber);
		  p = "     ZoneList = ";
		  for (zone = routingTableEntry->zoneList;
			   zone isnt empty;
			   zone = zone->next) {
			 printf("%s%s\n", p, zone->zone);
			 p = "                ";
		  }
	   }
	printf("********** RTMP routing table end.\n");
	
	HandleIncomingPackets();
	HandleDeferredTimerChecks();
	return;
	
}  // DumpRtmpRoutingTable
#endif

#endif
