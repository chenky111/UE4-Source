// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "IPAddressBSDIPv6.h"

#if PLATFORM_HAS_BSD_IPV6_SOCKETS

PRAGMA_DISABLE_DEPRECATION_WARNINGS

FInternetAddrBSDIPv6::FInternetAddrBSDIPv6()
{
	FMemory::Memzero(&Addr, sizeof(Addr));
	Addr.sin6_family = AF_INET6;
}

void FInternetAddrBSDIPv6::SetRawIp(const TArray<uint8>& RawAddr)
{
	if (RawAddr.Num() >= 16)
	{
		for (int i = 0; i < 16; ++i)
		{
			Addr.sin6_addr.s6_addr[i] = RawAddr[i];
		}

		// If this address has an interface, we'll copy it over too.
		if (RawAddr.Num() == 20)
		{
			Addr.sin6_scope_id = (RawAddr[16] << 0) | (RawAddr[17] << 8) | (RawAddr[18] << 16) | (RawAddr[19] << 24);
		}

		Addr.sin6_family = AF_INET6;
	}
	else
	{
		FMemory::Memzero(&Addr, sizeof(Addr));
		Addr.sin6_family = AF_INET6;
	}
}

TArray<uint8> FInternetAddrBSDIPv6::GetRawIp() const
{
	TArray<uint8> RawAddressArray;
	for (int i = 0; i < 16; ++i)
	{
		RawAddressArray.Add(Addr.sin6_addr.s6_addr[i]);
	}

	// Copy over the interface if we have it explicitly set (anything other than 0)
	if (Addr.sin6_scope_id != 0)
	{
		uint32 RawScopeId = Addr.sin6_scope_id;
		RawAddressArray.Add((RawScopeId >> 0) & 0xFF);
		RawAddressArray.Add((RawScopeId >> 8) & 0xFF);
		RawAddressArray.Add((RawScopeId >> 16) & 0xFF);
		RawAddressArray.Add((RawScopeId >> 24) & 0xFF);
	}
	return RawAddressArray;
}

void FInternetAddrBSDIPv6::SetIp(uint32 InAddr)
{
	if (InAddr == 0)
	{
		FMemory::Memzero(&Addr.sin6_addr, sizeof(Addr.sin6_addr));
	}
	else if (InAddr == IPv4MulitcastAddr)
	{
		// hack: if it's the hardcoded IPv4 multicasting address then translate into an IPv6 multicast address
		bool isValid;
		SetIp(TEXT("ff02::2"), isValid);
		check(isValid);
	}
	else
	{
		in_addr InternetAddr;
		InternetAddr.s_addr = htonl(InAddr);

		SetIp(InternetAddr);
	}
}

void FInternetAddrBSDIPv6::SetIp(const TCHAR* InAddr, bool& bIsValid)
{
	bIsValid = false;

	FString AddressString(InAddr);

	const bool bHasOpenBracket = AddressString.Contains("[");
	const int32 CloseBracketIndex = AddressString.Find("]");
	const bool bHasCloseBracket = CloseBracketIndex != -1;

	// IPv6 may or may not include open and close brackets.
	// However, only IPv6 address can have them.
	bool bIsIPv6 = bHasOpenBracket && bHasCloseBracket;

	// Valid IPv4 should not contain an open or close bracket
	const bool bIsLikelyIPv4 = !bHasOpenBracket && !bHasCloseBracket;

	if (bIsLikelyIPv4 || bIsIPv6)
	{
		const int32 LastColonIndex = AddressString.Find(":", ESearchCase::IgnoreCase, ESearchDir::FromEnd);

		// Double check to ensure this isn't actually an IPv6 address without brackets.
		if (bIsLikelyIPv4)
		{
			// IPv4 addresses can contain at most 1 colon.
			const int32 FirstColonIndex = AddressString.Find(":");
			bIsIPv6 = (FirstColonIndex != LastColonIndex);
		}

		// IPv4 address will only have a port when a colon is present.
		// IPv6 address will only have a port when surrounded by brackets.
		const bool bHasPort = (INDEX_NONE != LastColonIndex) && (!bIsIPv6 || (bHasCloseBracket && LastColonIndex > CloseBracketIndex));
		FString Port;
		if (bHasPort)
		{
			Port = AddressString.RightChop(LastColonIndex + 1);
			AddressString = AddressString.Left(LastColonIndex);
		}

		if (bIsIPv6)
		{
			AddressString.RemoveFromStart("[");
			AddressString.RemoveFromEnd("]");

			// Check for valid IPv6 address
			const auto InAddrAnsi = StringCast<ANSICHAR>(*AddressString);
#if PLATFORM_IOS
			ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
			if (SocketSubsystem && SocketSubsystem->GetHostByName(InAddrAnsi.Get(), *this) == SE_NO_ERROR)
			{
				bIsValid = true;
			}
			else
#endif
				if (inet_pton(AF_INET6, InAddrAnsi.Get(), &Addr.sin6_addr))
				{
					bIsValid = true;
				}
		}
		else
		{
			const auto InAddrAnsi = StringCast<ANSICHAR>(*AddressString);
			in_addr IPv4Addr;
#if PLATFORM_IOS
			ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
			if (SocketSubsystem && SocketSubsystem->GetHostByName(InAddrAnsi.Get(), *this) == SE_NO_ERROR)
			{
				bIsValid = true;
			}
			else
#endif
				if (inet_pton(AF_INET, InAddrAnsi.Get(), &IPv4Addr))
				{
					bIsValid = true;
					SetIp(IPv4Addr);
				}
		}

		if (bHasPort)
		{
			SetPort(FCString::Atoi(*Port));
		}
	}
}

void FInternetAddrBSDIPv6::SetIp(const in_addr& IPv4Addr)
{
	FMemory::Memzero(&Addr.sin6_addr, sizeof(Addr.sin6_addr));

	// map IPv4 to IPv6 address (only works on hybrid network stacks)
	Addr.sin6_addr.s6_addr[10] = 0xff;
	Addr.sin6_addr.s6_addr[11] = 0xff;

	uint8	IPv4b1 = (static_cast<uint32>(IPv4Addr.s_addr) & 0xFF),
		IPv4b2 = ((static_cast<uint32>(IPv4Addr.s_addr) >> 8) & 0xFF),
		IPv4b3 = ((static_cast<uint32>(IPv4Addr.s_addr) >> 16) & 0xFF),
		IPv4b4 = ((static_cast<uint32>(IPv4Addr.s_addr) >> 24) & 0xFF);

	Addr.sin6_addr.s6_addr[12] = IPv4b1;
	Addr.sin6_addr.s6_addr[13] = IPv4b2;
	Addr.sin6_addr.s6_addr[14] = IPv4b3;
	Addr.sin6_addr.s6_addr[15] = IPv4b4;

	UE_LOG(LogSockets, Verbose, TEXT("Using IPv4 address: %d.%d.%d.%d  on an ipv6 socket"),
		IPv4b1,
		IPv4b2,
		IPv4b3,
		IPv4b4
	);
}

void FInternetAddrBSDIPv6::SetIp(const in6_addr& IpAddr)
{
	Addr.sin6_addr = IpAddr;
}

void FInternetAddrBSDIPv6::SetIp(const sockaddr_storage& IpAddr)
{
	if (IpAddr.ss_family == AF_INET)
	{
		const sockaddr_in* SockAddr = (const sockaddr_in*)&IpAddr;
		SetIp(SockAddr->sin_addr);
	}
	else if (IpAddr.ss_family == AF_INET6)
	{
		const sockaddr_in6* SockAddr = (const sockaddr_in6*)&IpAddr;
		SetIp(SockAddr->sin6_addr);
	}
}

void FInternetAddrBSDIPv6::GetIp(uint32& OutAddr) const
{
	// grab the last 32-bits of the IPv6 address as this will correspond to the IPv4 address
	// in a dual stack system.
	// This function doesn't really make sense in IPv6, but too much other code relies on it
	// existing to not have this here.
#if PLATFORM_LITTLE_ENDIAN
	OutAddr = (Addr.sin6_addr.s6_addr[12] << 24) | (Addr.sin6_addr.s6_addr[13] << 16) | (Addr.sin6_addr.s6_addr[14] << 8) | (Addr.sin6_addr.s6_addr[15]);
#else
	OutAddr = (Addr.sin6_addr.s6_addr[15] << 24) | (Addr.sin6_addr.s6_addr[14] << 16) | (Addr.sin6_addr.s6_addr[13] << 8) | (Addr.sin6_addr.s6_addr[12]);
#endif
}

void FInternetAddrBSDIPv6::GetIp(in6_addr& OutAddr) const
{
	OutAddr = Addr.sin6_addr;
}

void FInternetAddrBSDIPv6::SetPort(int32 InPort)
{
	Addr.sin6_port = htons(InPort);
}

int32 FInternetAddrBSDIPv6::GetPort() const
{
	return ntohs(Addr.sin6_port);
}

void FInternetAddrBSDIPv6::SetAnyAddress()
{
	SetIp(in6addr_any);
	SetPort(0);
}

void FInternetAddrBSDIPv6::SetBroadcastAddress()
{
	// broadcast means something different in IPv6, but this is a rough equivalent
#ifndef in6addr_allnodesonlink
	// see RFC 4291, link-local multicast address http://tools.ietf.org/html/rfc4291
	static in6_addr in6addr_allnodesonlink =
	{
		{ { 0xff, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 } }
	};
#endif // in6addr_allnodesonlink

	SetIp(in6addr_allnodesonlink);
	SetPort(0);
}

void FInternetAddrBSDIPv6::SetLoopbackAddress()
{
	SetIp(in6addr_loopback);
	SetPort(0);
}

FString FInternetAddrBSDIPv6::ToString(bool bAppendPort) const
{
	char IPStr[INET6_ADDRSTRLEN];

	inet_ntop(AF_INET6, (void*)&Addr.sin6_addr, IPStr, INET6_ADDRSTRLEN);

	FString Result("[");
	Result += IPStr;
	Result += "]";

	if (bAppendPort)
	{
		Result += ":";
		Result += FString::Printf(TEXT("%d"), GetPort());
	}

	return Result;
}

uint32 FInternetAddrBSDIPv6::GetTypeHash()
{
	// @todo: Find a more efficient way to hash IPv6 addresses, if they are to be used with NetConnection's
	//return Addr.sin_addr.s_addr + (Addr.sin_port * 23);

	return ::GetTypeHash(*ToString(true));
}

bool FInternetAddrBSDIPv6::IsValid() const
{
	FInternetAddrBSDIPv6 Temp;
	return memcmp(&Addr.sin6_addr, &Temp.Addr.sin6_addr, sizeof(in6_addr)) != 0;
}

TSharedRef<FInternetAddr> FInternetAddrBSDIPv6::Clone() const
{

	FSocketSubsystemBSDIPv6* SocketSubsystem = static_cast<FSocketSubsystemBSDIPv6*>(ISocketSubsystem::Get());
	check(SocketSubsystem);

	TSharedRef<FInternetAddr> NewAddress = SocketSubsystem->CreateInternetAddr();
	((FInternetAddrBSDIPv6*)(&(NewAddress.Get())))->Addr = Addr;
	
	return NewAddress;
}

bool FInternetAddrBSDIPv6::operator==(const FInternetAddr& Other) const
{
	FInternetAddrBSDIPv6& OtherBSD = (FInternetAddrBSDIPv6&)Other;
	return memcmp(&Addr.sin6_addr, &OtherBSD.Addr.sin6_addr, sizeof(in6_addr)) == 0 &&
		Addr.sin6_port == OtherBSD.Addr.sin6_port &&
		Addr.sin6_family == OtherBSD.Addr.sin6_family;
}

FResolveInfoCachedBSDIPv6::FResolveInfoCachedBSDIPv6(const FInternetAddr& InAddr)
{
	
	const FInternetAddrBSDIPv6* InAddrAsIPv6 = static_cast<const FInternetAddrBSDIPv6*>(&InAddr);
	if (InAddrAsIPv6)
	{
		Addr = ISocketSubsystem::Get()->CreateInternetAddr();
		FInternetAddrBSDIPv6* AddrAsIPv6 = static_cast<FInternetAddrBSDIPv6*>(Addr.Get());
		if (AddrAsIPv6)
		{
			AddrAsIPv6->SetPort(InAddr.GetPort());
			in6_addr temp;
			InAddrAsIPv6->GetIp(temp);
			AddrAsIPv6->SetIp(temp);
		}
	}
	else
	{
		uint32 IpAddr;
		InAddr.GetIp(IpAddr);
		Addr = ISocketSubsystem::Get()->CreateInternetAddr(IpAddr, InAddr.GetPort());
	}
	
}

PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif