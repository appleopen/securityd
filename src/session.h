/*
 * Copyright (c) 2000-2004 Apple Computer, Inc. All Rights Reserved.
 * 
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */


//
// session - authentication session domains
//
#ifndef _H_SESSION
#define _H_SESSION

#include "securityserver.h"
#include "structure.h"
#include "acls.h"
#include "authority.h"
#include <Security/AuthSession.h>
#include <security_cdsa_utilities/handleobject.h>
#include <security_cdsa_utilities/cssmdb.h>

#if __GNUC__ > 2
#include <ext/hash_map>
using __gnu_cxx::hash_map;
#else
#include <hash_map>
#endif


class Key;
class Connection;


//
// A Session object represents one or more Connections that are known to
// belong to the same authentication domain. Informally this means just
// about "the same user", for the right definition of "user." The upshot
// is that global credentials can be shared by Connections of one Session
// with a modicum of security, and so Sessions are the natural nexus of
// single-sign-on functionality.
//
class Session : public HandleObject, public PerSession {
public:
    typedef MachPlusPlus::Bootstrap Bootstrap;

    Session(Bootstrap bootstrap, Port servicePort, SessionAttributeBits attrs = 0);
	virtual ~Session();
    
    Bootstrap bootstrapPort() const		{ return mBootstrap; }
	Port servicePort() const			{ return mServicePort; }
    
	virtual void release();
	
	IFDUMP(virtual void dumpNode());
    
public:
    static const SessionAttributeBits settableAttributes =
        sessionHasGraphicAccess | sessionHasTTY | sessionIsRemote;

    SessionAttributeBits attributes() const			{ return mAttributes; }
    bool attribute(SessionAttributeBits bits) const	{ return mAttributes & bits; }
    
    static void setup(SessionCreationFlags flags, SessionAttributeBits attrs);
    void setupAttributes(SessionAttributeBits attrs);
    
protected:
    void setAttributes(SessionAttributeBits attrs)	{ mAttributes |= attrs; }
    
public:
	const CredentialSet &authCredentials() const	{ return mSessionCreds; }

	OSStatus authCreate(const AuthItemSet &rights, const AuthItemSet &environment,
		AuthorizationFlags flags, AuthorizationBlob &newHandle, const security_token_t &securityToken);
	void authFree(const AuthorizationBlob &auth, AuthorizationFlags flags);
	OSStatus authGetRights(const AuthorizationBlob &auth,
		const AuthItemSet &requestedRights, const AuthItemSet &environment,
		AuthorizationFlags flags, AuthItemSet &grantedRights);
	OSStatus authGetInfo(const AuthorizationBlob &auth, const char *tag, AuthItemSet &contextInfo);
    
	OSStatus authExternalize(const AuthorizationBlob &auth, AuthorizationExternalForm &extForm);
	OSStatus authInternalize(const AuthorizationExternalForm &extForm, AuthorizationBlob &auth);

	OSStatus authorizationdbGet(AuthorizationString inRightName, CFDictionaryRef *rightDict);
	OSStatus authorizationdbSet(const AuthorizationBlob &authBlob, AuthorizationString inRightName, CFDictionaryRef rightDict);
	OSStatus authorizationdbRemove(const AuthorizationBlob &authBlob, AuthorizationString inRightName);

private:
    struct AuthorizationExternalBlob {
        AuthorizationBlob blob;
        mach_port_t session;
    };
	
protected:
    AuthorizationToken &authorization(const AuthorizationBlob &blob);
	void mergeCredentials(CredentialSet &creds);

public:
    static Session &find(Port servPort);
    static Session &find(SecuritySessionId id);
    static void destroy(Port servPort);
	
	static void processSystemSleep();
    
protected:
	mutable Mutex mLock;			// object lock
    
    Bootstrap mBootstrap;			// session bootstrap port
	Port mServicePort;				// SecurityServer service port for this session
    SessionAttributeBits mAttributes; // attribute bits (see AuthSession.h)
    bool mDying;					// session is dying

    mutable Mutex mCredsLock;	// lock for mSessionCreds
	CredentialSet mSessionCreds;	// shared session authorization credentials

	void kill();
	
private:
	static PortMap<Session> mSessions;
};


//
// The RootSession is the session (i.e. bootstrap dictionary) of system daemons that are
// started early and don't belong to anything more restrictive. The RootSession is considered
// immortal.
// Currently, telnet sessions et al also default into this session, but this will change
// (we hope).
//
class RootSession : public Session {
public:
    RootSession(Port servicePort, SessionAttributeBits attrs = 0);
};


//
// A DynamicSession is the default type of session object. We create one when a new
// Connection initializes whose bootstrap port we haven't seen before. These Sessions
// are torn down when their bootstrap object disappears (which happens when mach_init
// destroys it due to its requestor referent vanishing).
//
class DynamicSession : private ReceivePort, public Session {
public:
    DynamicSession(const Bootstrap &bootstrap);
	~DynamicSession();
	
protected:
	void release();
};


#endif //_H_SESSION
