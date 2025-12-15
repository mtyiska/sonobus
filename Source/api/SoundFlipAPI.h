// SPDX-License-Identifier: GPLv3-or-later WITH Appstore-exception
// Copyright (C) 2024 SoundFlip

#pragma once

#include <JuceHeader.h>
#include "SoundFlipAuth.h"

class SoundFlipAPI
{
public:
    SoundFlipAPI(SoundFlipAuth& auth);
    ~SoundFlipAPI();
    
    // Structs for API responses
    struct ConnectionInfo
    {
        String server;
        int port = 10998;
        String group;
        String password;
    };
    
    struct Participant
    {
        String userId;
        String username;
        String avatar;
        int64 joinedAt = 0;
        int64 leftAt = 0;
    };
    
    struct CollabSession
    {
        String id;
        String inviteCode;
        String name;
        String status;
        String inviteUrl;
        int stemCount = 0;
        int durationSeconds = 0;
        ConnectionInfo connection;
        String createdById;
        String createdByUsername;
        String createdByAvatar;
        Array<Participant> participants;
        int64 createdAt = 0;
        int64 endedAt = 0;
    };
    
    struct Stem
    {
        String id;
        String filename;
        String downloadUrl;
        int64 sizeBytes = 0;
        int durationSeconds = 0;
        String uploadedById;
        String uploadedByUsername;
        String uploadedByAvatar;
        int64 createdAt = 0;
    };
    
    struct UploadUrlResponse
    {
        String uploadUrl;
        String stemId;
        String s3Key;
        int expiresIn = 3600;
    };
    
    // Collab Session Management
    CollabSession createCollabSession(const String& name = "");
    CollabSession getCollabSession(const String& sessionId);
    CollabSession getCollabSessionByInviteCode(const String& inviteCode);
    CollabSession joinCollabSession(const String& sessionIdOrInviteCode);
    bool leaveCollabSession(const String& sessionId);
    CollabSession updateCollabSession(const String& sessionId, 
                                       const String& name = "", 
                                       const String& status = "");
    Array<CollabSession> listCollabSessions(const String& status = "", 
                                             int limit = 10, 
                                             int offset = 0);
    
    // Stem Management
    UploadUrlResponse requestStemUploadUrl(const String& sessionId, 
                                            const String& filename, 
                                            const String& contentType,
                                            int64 sizeBytes);
    Stem completeStemUpload(const String& sessionId, 
                             const String& stemId, 
                             int durationSeconds = 0);
    Array<Stem> listSessionStems(const String& sessionId);
    bool deleteStem(const String& sessionId, const String& stemId);
    
    // S3 Upload Helper
    bool uploadFileToS3(const String& presignedUrl, 
                         const File& file, 
                         const String& contentType);
    
    //==========================================================================
    // Convenience aliases for SessionManager compatibility
    //==========================================================================
    
    /** Convenience wrapper for requestStemUploadUrl with default sizeBytes */
    UploadUrlResponse getUploadUrl(const String& sessionId, 
                                    const String& filename, 
                                    const String& contentType)
    {
        return requestStemUploadUrl(sessionId, filename, contentType, 0);
    }
    
    /** Convenience wrapper for completeStemUpload that returns bool */
    bool markStemComplete(const String& sessionId, 
                          const String& stemId, 
                          int64 fileSize)
    {
        // fileSize is not used by completeStemUpload, it uses durationSeconds instead
        ignoreUnused(fileSize);
        auto result = completeStemUpload(sessionId, stemId, 0);
        return result.id.isNotEmpty();
    }
    
    // Error handling
    String getLastError() const { return lastError; }
    int getLastStatusCode() const { return lastStatusCode; }
    
private:
    var makeRequest(const String& endpoint, 
                    const String& method = "GET",
                    const var& body = var());
    
    // JSON parsing helpers
    CollabSession parseCollabSession(const var& json);
    Participant parseParticipant(const var& json);
    ConnectionInfo parseConnectionInfo(const var& json);
    Stem parseStem(const var& json);
    UploadUrlResponse parseUploadUrlResponse(const var& json);
    
    SoundFlipAuth& auth;
    String apiBaseUrl = "https://api.soundflip.io";
    String lastError;
    int lastStatusCode = 0;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoundFlipAPI)
};