// SPDX-License-Identifier: GPLv3-or-later WITH Appstore-exception
// Copyright (C) 2024 SoundFlip

#include "SessionManager.h"

SessionManager::SessionManager(SoundFlipAPI& apiRef)
    : api(apiRef)
{
}

bool SessionManager::createSession(const String& name)
{
    if (currentState != State::Idle && currentState != State::Error)
    {
        lastError = "Already in a session";
        return false;
    }
    
    setState(State::CreatingSession);
    
    String sessionName = name.isEmpty() ? "SoundFlip Session" : name;
    
    // Use the API's createCollabSession method
    auto result = api.createCollabSession(sessionName);
    
    if (result.id.isEmpty())
    {
        lastError = api.getLastError().isEmpty() ? "Failed to create session" : api.getLastError();
        setState(State::Error);
        return false;
    }
    
    currentSessionId = result.id;
    currentSessionName = result.name;
    currentInviteUrl = result.inviteUrl;
    
    connectionInfo.server = result.connection.server;
    connectionInfo.port = result.connection.port;
    connectionInfo.group = result.connection.group;
    connectionInfo.password = result.connection.password;
    
    DBG("SessionManager: Created session " + currentSessionId);
    DBG("SessionManager: Connection - " + connectionInfo.server + ":" + 
        String(connectionInfo.port) + " group: " + connectionInfo.group);
    
    setState(State::Connecting);
    
    return true;
}

bool SessionManager::joinSession(const String& inviteCodeOrUrl)
{
    if (currentState != State::Idle && currentState != State::Error)
    {
        lastError = "Already in a session";
        return false;
    }
    
    setState(State::JoiningSession);
    
    String sessionCode = extractSessionCode(inviteCodeOrUrl);
    
    if (sessionCode.isEmpty())
    {
        lastError = "Invalid invite code or URL";
        setState(State::Error);
        return false;
    }
    
    // Use the API's joinCollabSession method
    auto result = api.joinCollabSession(sessionCode);
    
    if (result.id.isEmpty())
    {
        lastError = api.getLastError().isEmpty() ? "Failed to join session" : api.getLastError();
        setState(State::Error);
        return false;
    }
    
    currentSessionId = result.id;
    currentSessionName = result.name;
    currentInviteUrl = result.inviteUrl;
    
    connectionInfo.server = result.connection.server;
    connectionInfo.port = result.connection.port;
    connectionInfo.group = result.connection.group;
    connectionInfo.password = result.connection.password;
    
    DBG("SessionManager: Joined session " + currentSessionId + " (" + currentSessionName + ")");
    
    setState(State::Connecting);
    
    return true;
}

void SessionManager::leaveSession()
{
    if (currentState == State::Idle)
        return;
    
    setState(State::Disconnecting);
    
    // Notify API that we're leaving (for tracking purposes)
    if (currentSessionId.isNotEmpty())
    {
        api.leaveCollabSession(currentSessionId);
    }
    
    clearSession();
    setState(State::Idle);
    
    sendChangeMessage();
}

void SessionManager::fetchRecentSessions(int limit)
{
    // Fetch from API
    auto sessions = api.listCollabSessions("", limit, 0);
    
    recentSessions.clear();
    
    for (const auto& session : sessions)
    {
        RecentSessionInfo info;
        info.id = session.id;
        info.name = session.name;
        info.status = session.status;
        info.stemCount = session.stemCount;
        info.createdAt = session.createdAt;
        
        // Convert participants
        for (const auto& p : session.participants)
        {
            SessionParticipant participant;
            participant.odid = p.userId;
            participant.username = p.username;
            info.participants.add(participant);
        }
        
        recentSessions.add(info);
    }
    
    // Notify listeners that data has changed
    sendChangeMessage();
}

bool SessionManager::uploadStem(const String& sessionId,
                                 const File& audioFile,
                                 std::function<void(float progress)> progressCallback,
                                 String& outError)
{
    DBG("SessionManager::uploadStem - Starting upload for session: " + sessionId);
    DBG("SessionManager::uploadStem - File: " + audioFile.getFullPathName());
    
    if (!audioFile.existsAsFile())
    {
        outError = "Audio file does not exist";
        return false;
    }
    
    String useSessionId = sessionId.isEmpty() ? currentSessionId : sessionId;
    if (useSessionId.isEmpty())
    {
        outError = "No session ID provided";
        return false;
    }
    
    // Step 1: Get presigned upload URL from API
    if (progressCallback)
        progressCallback(0.1f);
    
    String filename = audioFile.getFileName();
    int64 fileSize = audioFile.getSize();
    
    // Determine content type based on extension
    String contentType = "audio/flac";
    String ext = audioFile.getFileExtension().toLowerCase();
    if (ext == ".wav")
        contentType = "audio/wav";
    else if (ext == ".mp3")
        contentType = "audio/mpeg";
    else if (ext == ".ogg")
        contentType = "audio/ogg";
    
    DBG("SessionManager::uploadStem - Requesting upload URL for: " + filename + " (" + String(fileSize) + " bytes)");
    
    // Use the existing API method
    auto uploadInfo = api.requestStemUploadUrl(useSessionId, filename, contentType, fileSize);
    
    if (uploadInfo.uploadUrl.isEmpty() || uploadInfo.stemId.isEmpty())
    {
        outError = api.getLastError().isEmpty() ? "Failed to get upload URL" : api.getLastError();
        DBG("SessionManager::uploadStem - Failed to get upload URL: " + outError);
        return false;
    }
    
    DBG("SessionManager::uploadStem - Got upload URL, stemId: " + uploadInfo.stemId);
    
    if (progressCallback)
        progressCallback(0.2f);
    
    // Step 2: Upload file to S3 using the API's helper method
    DBG("SessionManager::uploadStem - Uploading to S3...");
    
    if (progressCallback)
        progressCallback(0.5f);
    
    bool uploadSuccess = api.uploadFileToS3(uploadInfo.uploadUrl, audioFile, contentType);
    
    if (!uploadSuccess)
    {
        outError = api.getLastError().isEmpty() ? "Failed to upload to S3" : api.getLastError();
        DBG("SessionManager::uploadStem - S3 upload failed: " + outError);
        return false;
    }
    
    if (progressCallback)
        progressCallback(0.8f);
    
    // Step 3: Mark upload as complete using the existing API method
    DBG("SessionManager::uploadStem - Marking upload complete...");
    
    // Calculate approximate duration (assuming 44100 Hz sample rate for estimation)
    int durationSeconds = 0;  // Let the server calculate from the file
    
    auto completedStem = api.completeStemUpload(useSessionId, uploadInfo.stemId, durationSeconds);
    
    if (completedStem.id.isEmpty())
    {
        outError = api.getLastError().isEmpty() ? "Failed to mark upload complete" : api.getLastError();
        DBG("SessionManager::uploadStem - Failed to mark complete: " + outError);
        return false;
    }
    
    if (progressCallback)
        progressCallback(1.0f);
    
    DBG("SessionManager::uploadStem - Upload completed successfully!");
    return true;
}

void SessionManager::onSessionConnected()
{
    DBG("SessionManager: Session connected");
    setState(State::Connected);
    
    if (onSessionConnectedCallback)
        onSessionConnectedCallback();
    
    sendChangeMessage();
}

void SessionManager::onSessionDisconnected()
{
    DBG("SessionManager: Session disconnected");
    
    State previousState = currentState;
    clearSession();
    setState(State::Idle);
    
    // Only trigger callback if we were previously connected
    if (previousState == State::Connected)
    {
        if (onSessionDisconnectedCallback)
            onSessionDisconnectedCallback();
    }
    
    sendChangeMessage();
}

void SessionManager::onConnectionFailed(const String& error)
{
    DBG("SessionManager: Connection failed - " + error);
    lastError = error;
    
    clearSession();
    setState(State::Error);
    
    if (onConnectionFailedCallback)
        onConnectionFailedCallback(error);
    
    sendChangeMessage();
}

void SessionManager::onPeerJoined(const String& username)
{
    DBG("SessionManager: Peer joined - " + username);
    
    // Add to participants list
    SessionParticipant participant;
    participant.username = username;
    participants.add(participant);
    
    sendChangeMessage();
}

void SessionManager::onPeerLeft(const String& username)
{
    DBG("SessionManager: Peer left - " + username);
    
    // Remove from participants list
    for (int i = participants.size() - 1; i >= 0; --i)
    {
        if (participants[i].username == username)
        {
            participants.remove(i);
            break;
        }
    }
    
    sendChangeMessage();
}

void SessionManager::setState(State newState)
{
    if (currentState != newState)
    {
        DBG("SessionManager: State change " + String((int)currentState) + " -> " + String((int)newState));
        currentState = newState;
    }
}

void SessionManager::clearSession()
{
    currentSessionId = "";
    currentSessionName = "";
    currentInviteUrl = "";
    connectionInfo = SessionConnectionInfo();
    participants.clear();
    lastError = "";
}

String SessionManager::extractSessionCode(const String& input)
{
    String trimmed = input.trim();
    
    // Check if it's a URL
    if (trimmed.containsChar('/'))
    {
        // Extract last path component as session code
        int lastSlash = trimmed.lastIndexOf("/");
        if (lastSlash >= 0 && lastSlash < trimmed.length() - 1)
        {
            return trimmed.substring(lastSlash + 1);
        }
    }
    
    // Assume it's a direct code
    return trimmed;
}