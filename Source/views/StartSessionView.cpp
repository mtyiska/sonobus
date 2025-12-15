// SPDX-License-Identifier: GPLv3-or-later WITH Appstore-exception
// Copyright (C) 2024 SoundFlip

#include "StartSessionView.h"
#include "../managers/SessionManager.h"
#include "../SonobusPluginEditor.h"

StartSessionView::StartSessionView()
{
    setupUI();
}

StartSessionView::StartSessionView(SessionManager* sm, SonobusAudioProcessorEditor* ed)
    : sessionManager(sm), editor(ed)
{
    setupUI();
    
    // Get processor from editor if available
    if (editor)
    {
        processor = dynamic_cast<SonobusAudioProcessor*>(editor->getAudioProcessor());
        if (processor)
        {
            processor->addClientListener(this);
        }
    }
}

StartSessionView::~StartSessionView()
{
    stopTimer();
    if (processor)
    {
        processor->removeClientListener(this);
    }
}

void StartSessionView::setProcessor(SonobusAudioProcessor* proc)
{
    // Unregister from old processor
    if (processor)
    {
        processor->removeClientListener(this);
    }
    
    processor = proc;
    
    // Register with new processor
    if (processor)
    {
        processor->addClientListener(this);
    }
}

void StartSessionView::setupUI()
{
    titleLabel.setText("Start New Session", dontSendNotification);
    titleLabel.setFont(Font(24.0f, Font::bold));
    titleLabel.setJustificationType(Justification::centred);
    titleLabel.setColour(Label::textColourId, Colours::white);
    addAndMakeVisible(titleLabel);

    sessionNameLabel.setText("Session Name:", dontSendNotification);
    sessionNameLabel.setColour(Label::textColourId, Colours::white);
    addAndMakeVisible(sessionNameLabel);

    sessionNameEditor.setTextToShowWhenEmpty("Friday night cookup...", Colours::grey);
    addAndMakeVisible(sessionNameEditor);
    
    statusLabel.setText("", dontSendNotification);
    statusLabel.setJustificationType(Justification::centred);
    statusLabel.setColour(Label::textColourId, Colours::orange);
    addAndMakeVisible(statusLabel);

    createButton.setButtonText("Create Session");
    createButton.setColour(TextButton::buttonColourId, Colour(0xff6c5ce7));
    createButton.onClick = [this]() {
        handleCreateSession();
    };
    addAndMakeVisible(createButton);

    backButton.setButtonText("Back");
    backButton.onClick = [this]() {
        cleanupConnection();
        if (onBackClicked)
            onBackClicked();
    };
    addAndMakeVisible(backButton);
}

void StartSessionView::handleCreateSession()
{
    DBG("=== StartSessionView::handleCreateSession ===");
    
    if (isCreatingSession || isWaitingForConnect || isWaitingForGroupJoin)
    {
        DBG("  Already in progress, ignoring");
        return;
    }
    
    if (!sessionManager)
    {
        DBG("  ERROR: sessionManager is nullptr!");
        showError("Session manager not available");
        return;
    }
    
    if (!processor)
    {
        DBG("  ERROR: processor is nullptr!");
        // Try to get it from editor
        if (editor)
        {
            processor = dynamic_cast<SonobusAudioProcessor*>(editor->getAudioProcessor());
            if (processor)
            {
                processor->addClientListener(this);
            }
        }
        
        if (!processor)
        {
            showError("Audio processor not available");
            return;
        }
    }
    
    String sessionName = sessionNameEditor.getText().trim();
    DBG("  Session name: " + sessionName);
    
    // Update UI state
    isCreatingSession = true;
    setUIEnabled(false);
    showStatus("Creating session...");
    
    // Create session via API
    DBG("  Calling sessionManager->createSession()...");
    bool success = sessionManager->createSession(sessionName);
    DBG("  createSession returned: " + String(success ? "true" : "false"));
    
    if (!success)
    {
        DBG("  ERROR: createSession failed! Error: " + sessionManager->getLastError());
        isCreatingSession = false;
        setUIEnabled(true);
        showError(sessionManager->getLastError());
        return;
    }
    
    DBG("  Session created successfully!");
    DBG("  Session ID: " + sessionManager->getCurrentSessionId());
    showStatus("Connecting to server...");
    
    // Get connection info from SessionManager
    auto connectionInfo = sessionManager->getConnectionInfo();
    
    DBG("=== CONNECTION INFO ===");
    DBG("  Server: " + connectionInfo.server);
    DBG("  Port: " + String(connectionInfo.port));
    DBG("  Group: " + connectionInfo.group);
    DBG("  Password: " + connectionInfo.password);
    DBG("=======================");
    
    // Validate connection info
    if (connectionInfo.server.isEmpty())
    {
        DBG("  ERROR: Server is empty!");
        isCreatingSession = false;
        setUIEnabled(true);
        showError("No server address received from API");
        return;
    }
    
    if (connectionInfo.group.isEmpty())
    {
        DBG("  ERROR: Group is empty!");
        isCreatingSession = false;
        setUIEnabled(true);
        showError("No group name received from API");
        return;
    }
    
    // Store for later use in callbacks
    pendingSessionId = sessionManager->getCurrentSessionId();
    pendingGroupName = connectionInfo.group;
    pendingGroupPassword = connectionInfo.password;
    
    // Get current username from processor or use a default
    String username = processor->getCurrentUsername();
    if (username.isEmpty())
    {
        username = "User_" + String(Random::getSystemRandom().nextInt(9999));
    }
    DBG("  Username: " + username);
    
    // Disconnect first if already connected to a different server/group
    if (processor->isConnectedToServer())
    {
        DBG("  Already connected, disconnecting first...");
        processor->disconnectFromServer();
    }
    
    // Start timeout timer
    connectionCheckCount = 0;
    startTimer(100);
    
    // Initiate connection to AOO server - this is ASYNC!
    isWaitingForConnect = true;
    isCreatingSession = false;
    
    DBG("  Initiating connection to " + connectionInfo.server + ":" + String(connectionInfo.port));
    bool initiated = processor->connectToServer(
        connectionInfo.server,
        connectionInfo.port,
        username,
        "" // user password (not group password)
    );
    
    if (!initiated)
    {
        DBG("  ERROR: connectToServer() returned false!");
        stopTimer();
        isWaitingForConnect = false;
        setUIEnabled(true);
        showError("Failed to initiate server connection");
        return;
    }
    
    DBG("  Connection initiated, waiting for callback...");
    // Now we wait for aooClientConnected callback...
}

void StartSessionView::aooClientConnected(SonobusAudioProcessor* proc, bool success, const String& errmesg)
{
    // Make sure this is for our connection attempt
    if (!isWaitingForConnect || proc != processor)
        return;
    
    isWaitingForConnect = false;
    
    DBG("=== aooClientConnected callback ===");
    DBG("  success: " + String(success ? "yes" : "no"));
    DBG("  error: " + errmesg);
    
    if (!success)
    {
        stopTimer();
        setUIEnabled(true);
        showError("Connection failed: " + (errmesg.isEmpty() ? "Unknown error" : errmesg));
        return;
    }
    
    DBG("  Connected to server, now joining group: " + pendingGroupName);
    showStatus("Joining session...");
    
    // NOW we can join the group since we're connected
    isWaitingForGroupJoin = true;
    
    bool joinInitiated = processor->joinServerGroup(
        pendingGroupName,
        pendingGroupPassword,
        false // not public
    );
    
    if (!joinInitiated)
    {
        DBG("  ERROR: joinServerGroup() returned false!");
        stopTimer();
        isWaitingForGroupJoin = false;
        processor->disconnectFromServer();
        setUIEnabled(true);
        showError("Failed to initiate group join");
        return;
    }
    
    DBG("  Group join initiated, waiting for callback...");
    // Now we wait for aooClientGroupJoined callback...
}

void StartSessionView::aooClientDisconnected(SonobusAudioProcessor* proc, bool success, const String& errmesg)
{
    // Handle unexpected disconnection during our connection flow
    if ((isWaitingForConnect || isWaitingForGroupJoin) && proc == processor)
    {
        DBG("=== aooClientDisconnected (unexpected) ===");
        DBG("  error: " + errmesg);
        stopTimer();
        isWaitingForConnect = false;
        isWaitingForGroupJoin = false;
        setUIEnabled(true);
        showError("Disconnected: " + (errmesg.isEmpty() ? "Connection lost" : errmesg));
    }
}

void StartSessionView::aooClientGroupJoined(SonobusAudioProcessor* proc, bool success, const String& group, const String& errmesg)
{
    // Make sure this is for our group join attempt
    if (!isWaitingForGroupJoin || proc != processor || group != pendingGroupName)
        return;
    
    stopTimer();
    isWaitingForGroupJoin = false;
    
    DBG("=== aooClientGroupJoined callback ===");
    DBG("  group: " + group);
    DBG("  success: " + String(success ? "yes" : "no"));
    DBG("  error: " + errmesg);
    
    if (!success)
    {
        processor->disconnectFromServer();
        setUIEnabled(true);
        showError("Failed to join session: " + (errmesg.isEmpty() ? "Unknown error" : errmesg));
        return;
    }
    
    DBG("  Successfully joined group: " + group);
    
    showStatus("Connected!");
    statusLabel.setColour(Label::textColourId, Colours::green);
    
    // Small delay to show success message before transitioning
    Timer::callAfterDelay(300, [this]() {
        if (onStartClicked)
            onStartClicked();
        
        if (onSessionCreated)
            onSessionCreated(pendingSessionId);
        
        if (onSessionStarted)
            onSessionStarted();
    });
}

void StartSessionView::timerCallback()
{
    connectionCheckCount++;
    
    // Timeout check
    if (connectionCheckCount >= maxConnectionChecks)
    {
        DBG("=== Connection timeout ===");
        DBG("  Timeout after " + String(connectionCheckCount * 100) + "ms");
        cleanupConnection();
        showError("Connection timed out");
        return;
    }
    
    // Update status with animated dots
    int dots = (connectionCheckCount / 5) % 4;
    String dotStr = String::repeatedString(".", dots);
    
    if (isWaitingForConnect)
    {
        showStatus("Connecting to server" + dotStr);
    }
    else if (isWaitingForGroupJoin)
    {
        showStatus("Joining session" + dotStr);
    }
}

void StartSessionView::cleanupConnection()
{
    DBG("=== cleanupConnection ===");
    stopTimer();
    
    bool wasConnecting = isWaitingForConnect || isWaitingForGroupJoin;
    
    isWaitingForConnect = false;
    isWaitingForGroupJoin = false;
    isCreatingSession = false;
    
    if (wasConnecting && processor)
    {
        if (processor->isConnectedToServer())
        {
            if (!pendingGroupName.isEmpty())
            {
                DBG("  Leaving group: " + pendingGroupName);
                processor->leaveServerGroup(pendingGroupName);
            }
            DBG("  Disconnecting from server");
            processor->disconnectFromServer();
        }
    }
    
    setUIEnabled(true);
}

void StartSessionView::showError(const String& message)
{
    DBG("StartSessionView Error: " + message);
    statusLabel.setText(message.isEmpty() ? "An error occurred" : message, dontSendNotification);
    statusLabel.setColour(Label::textColourId, Colour(0xffe74c3c));
}

void StartSessionView::showStatus(const String& message)
{
    statusLabel.setText(message, dontSendNotification);
    statusLabel.setColour(Label::textColourId, Colours::white);
}

void StartSessionView::setUIEnabled(bool enabled)
{
    sessionNameEditor.setEnabled(enabled);
    createButton.setEnabled(enabled);
    backButton.setEnabled(enabled);
    
    createButton.setButtonText(enabled ? "Create Session" : "Creating...");
}

void StartSessionView::paint(Graphics& g)
{
    g.fillAll(Colour(0xff1a1a2e));
}

void StartSessionView::resized()
{
    auto bounds = getLocalBounds().reduced(40);
    
    titleLabel.setBounds(bounds.removeFromTop(40));
    bounds.removeFromTop(30);
    
    auto row = bounds.removeFromTop(30);
    sessionNameLabel.setBounds(row.removeFromLeft(120));
    sessionNameEditor.setBounds(row);
    
    bounds.removeFromTop(20);
    
    statusLabel.setBounds(bounds.removeFromTop(25));
    
    bounds.removeFromTop(20);
    
    int buttonWidth = 150;
    auto buttonArea = bounds.removeFromTop(40);
    int totalWidth = buttonWidth * 2 + 20;
    int startX = (buttonArea.getWidth() - totalWidth) / 2;
    
    backButton.setBounds(buttonArea.getX() + startX, buttonArea.getY(), buttonWidth, 40);
    createButton.setBounds(buttonArea.getX() + startX + buttonWidth + 20, buttonArea.getY(), buttonWidth, 40);
}