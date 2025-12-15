// SPDX-License-Identifier: GPLv3-or-later WITH Appstore-exception
// Copyright (C) 2024 SoundFlip

#pragma once

#include <JuceHeader.h>
#include "../SonobusPluginProcessor.h"

class SessionManager;
class SonobusAudioProcessorEditor;

class StartSessionView : public Component,
                         public SonobusAudioProcessor::ClientListener,
                         private Timer
{
public:
    // Default constructor (for ScreenManager usage)
    StartSessionView();
    
    // Full constructor (for SonobusPluginEditor usage)
    StartSessionView(SessionManager* sessionManager, SonobusAudioProcessorEditor* editor);
    
    ~StartSessionView() override;

    void paint(Graphics& g) override;
    void resized() override;
    
    // Set processor for connection handling
    void setProcessor(SonobusAudioProcessor* proc);
    
    // Set session manager
    void setSessionManager(SessionManager* sm) { sessionManager = sm; }

    // Callbacks
    std::function<void()> onBackClicked;
    std::function<void()> onStartClicked;
    std::function<void(const String&)> onSessionCreated;
    std::function<void()> onSessionStarted;

    // SonobusAudioProcessor::ClientListener overrides
    void aooClientConnected(SonobusAudioProcessor* comp, bool success, const String& errmesg = "") override;
    void aooClientDisconnected(SonobusAudioProcessor* comp, bool success, const String& errmesg = "") override;
    void aooClientGroupJoined(SonobusAudioProcessor* comp, bool success, const String& group, const String& errmesg = "") override;

private:
    void setupUI();
    void handleCreateSession();
    void showError(const String& message);
    void showStatus(const String& message);
    void setUIEnabled(bool enabled);
    void cleanupConnection();
    
    // Timer callback for connection timeout
    void timerCallback() override;

    SessionManager* sessionManager = nullptr;
    SonobusAudioProcessorEditor* editor = nullptr;
    SonobusAudioProcessor* processor = nullptr;

    Label titleLabel;
    Label sessionNameLabel;
    TextEditor sessionNameEditor;
    Label statusLabel;
    TextButton createButton;
    TextButton backButton;

    bool isCreatingSession = false;
    bool isWaitingForConnect = false;
    bool isWaitingForGroupJoin = false;
    
    String pendingSessionId;
    String pendingGroupName;
    String pendingGroupPassword;
    
    int connectionCheckCount = 0;
    static constexpr int maxConnectionChecks = 300; // 30 seconds at 100ms intervals

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StartSessionView)
};