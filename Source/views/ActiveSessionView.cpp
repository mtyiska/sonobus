#include "ActiveSessionView.h"
#include "../managers/SessionManager.h"
#include "../SonobusPluginEditor.h"
#include "../api/SoundFlipAPI.h"

ActiveSessionView::ActiveSessionView()
    : sessionManager(nullptr), editor(nullptr), api(nullptr)
{
    setupUI();
}

ActiveSessionView::ActiveSessionView(SessionManager* sm, SonobusAudioProcessorEditor* ed)
    : sessionManager(sm), editor(ed), api(nullptr)
{
    if (sessionManager)
        sessionManager->addChangeListener(this);
    
    setupUI();
    
    if (sessionManager && sessionManager->isConnected())
    {
        currentSessionId = sessionManager->getCurrentSessionId();
        setSessionInfo(sessionManager->getCurrentSessionName(),
                       sessionManager->getInviteUrl());
        updateParticipantsUI();
    }
}

ActiveSessionView::ActiveSessionView(SessionManager* sm, 
                                     SonobusAudioProcessorEditor* ed,
                                     SoundFlipAPI* apiRef)
    : sessionManager(sm), editor(ed), api(apiRef)
{
    if (sessionManager)
        sessionManager->addChangeListener(this);
    
    setupUI();
    
    if (sessionManager && sessionManager->isConnected())
    {
        currentSessionId = sessionManager->getCurrentSessionId();
        setSessionInfo(sessionManager->getCurrentSessionName(),
                       sessionManager->getInviteUrl());
        startTimer(recordingTimerIntervalMs);
        lastParticipantPollTime = Time::getMillisecondCounterHiRes();
        updateParticipantsUI();
    }
}

ActiveSessionView::~ActiveSessionView()
{
    stopTimer();
    if (sessionManager)
        sessionManager->removeChangeListener(this);
}

void ActiveSessionView::setupUI()
{
    titleLabel.setText("Active Session", dontSendNotification);
    titleLabel.setFont(Font(24.0f, Font::bold));
    titleLabel.setJustificationType(Justification::centred);
    titleLabel.setColour(Label::textColourId, Colours::white);
    addAndMakeVisible(titleLabel);

    statusLabel.setText("Connected", dontSendNotification);
    statusLabel.setJustificationType(Justification::centred);
    statusLabel.setColour(Label::textColourId, Colours::green);
    addAndMakeVisible(statusLabel);
    
    participantsLabel.setText("Participants:", dontSendNotification);
    participantsLabel.setFont(Font(14.0f, Font::bold));
    participantsLabel.setColour(Label::textColourId, Colours::white);
    addAndMakeVisible(participantsLabel);
    
    participantListLabel.setText("Loading...", dontSendNotification);
    participantListLabel.setColour(Label::textColourId, Colour(0xffaaaaaa));
    addAndMakeVisible(participantListLabel);
    
    // Recording timer label (initially hidden)
    recordingTimerLabel.setText("00:00", dontSendNotification);
    recordingTimerLabel.setFont(Font(18.0f, Font::bold));
    recordingTimerLabel.setJustificationType(Justification::centred);
    recordingTimerLabel.setColour(Label::textColourId, Colour(0xffe74c3c));
    recordingTimerLabel.setVisible(false);
    addAndMakeVisible(recordingTimerLabel);

    recordButton.setButtonText("Record");
    recordButton.setColour(TextButton::buttonColourId, Colour(0xff6c5ce7));
    recordButton.onClick = [this]() {
        handleRecordClicked();
    };
    addAndMakeVisible(recordButton);

    chatButton.setButtonText("Chat");
    chatButton.onClick = [this]() {
        if (onChatClicked)
            onChatClicked();
    };
    addAndMakeVisible(chatButton);

    inviteButton.setButtonText("Invite");
    inviteButton.setColour(TextButton::buttonColourId, Colour(0xff00cec9));
    inviteButton.onClick = [this]() {
        handleInviteClicked();
    };
    addAndMakeVisible(inviteButton);

    endSessionButton.setButtonText("Leave Session");
    endSessionButton.setColour(TextButton::buttonColourId, Colour(0xffe74c3c));
    endSessionButton.onClick = [this]() {
        handleEndSession();
    };
    addAndMakeVisible(endSessionButton);
}

void ActiveSessionView::setSoundFlipAPI(SoundFlipAPI* apiRef)
{
    api = apiRef;
}

void ActiveSessionView::setSessionManager(SessionManager* sm)
{
    if (sessionManager)
        sessionManager->removeChangeListener(this);
    
    sessionManager = sm;
    
    if (sessionManager)
    {
        sessionManager->addChangeListener(this);
        
        if (sessionManager->isConnected())
        {
            currentSessionId = sessionManager->getCurrentSessionId();
            setSessionInfo(sessionManager->getCurrentSessionName(),
                           sessionManager->getInviteUrl());
            
            startTimer(recordingTimerIntervalMs);
            lastParticipantPollTime = Time::getMillisecondCounterHiRes();
            
            updateParticipantsUI();
        }
    }
}

void ActiveSessionView::setSessionInfo(const String& name, const String& inviteUrl)
{
    currentSessionName = name;
    currentInviteUrl = inviteUrl;
    
    if (currentSessionName.isNotEmpty())
        titleLabel.setText(currentSessionName, dontSendNotification);
    else
        titleLabel.setText("Active Session", dontSendNotification);
}

void ActiveSessionView::refreshParticipants()
{
    fetchAndUpdateParticipants();
}

void ActiveSessionView::clearRecordingInfo()
{
    isRecording = false;
    recordingStartTime = 0.0;
    recordingDuration = 0.0;
    recordedFilePath = "";
    recordingTimerLabel.setVisible(false);
    recordButton.setButtonText("Record");
    recordButton.setColour(TextButton::buttonColourId, Colour(0xff6c5ce7));
}

void ActiveSessionView::timerCallback()
{
    double currentTime = Time::getMillisecondCounterHiRes();
    
    // Update recording timer display if recording
    if (isRecording)
    {
        updateRecordingTimerDisplay();
    }
    
    // Poll participants at the slower interval
    if (api && !currentSessionId.isEmpty())
    {
        if (currentTime - lastParticipantPollTime >= pollIntervalMs)
        {
            fetchAndUpdateParticipants();
            lastParticipantPollTime = currentTime;
        }
    }
}

void ActiveSessionView::updateRecordingTimerDisplay()
{
    if (isRecording)
    {
        double elapsed = (Time::getMillisecondCounterHiRes() - recordingStartTime) / 1000.0;
        recordingTimerLabel.setText(formatDuration(elapsed), dontSendNotification);
    }
}

String ActiveSessionView::formatDuration(double seconds) const
{
    int totalSeconds = static_cast<int>(seconds);
    int minutes = totalSeconds / 60;
    int secs = totalSeconds % 60;
    
    return String::formatted("%02d:%02d", minutes, secs);
}

void ActiveSessionView::handleRecordClicked()
{
    if (!editor)
    {
        DBG("ActiveSessionView: No editor pointer, cannot control recording");
        if (onRecordClicked)
            onRecordClicked();
        return;
    }
    
    // Access processor through the editor's processor member
    // SonobusAudioProcessorEditor inherits from AudioProcessorEditor which has getAudioProcessor()
    auto* processorPtr = dynamic_cast<SonobusAudioProcessor*>(editor->getAudioProcessor());
    if (!processorPtr)
    {
        DBG("ActiveSessionView: Could not get processor");
        return;
    }
    auto& processor = *processorPtr;
    
    if (!isRecording)
    {
        // Start recording
        DBG("ActiveSessionView: Starting recording...");
        
        // Generate a filename based on session name and timestamp
        String timestamp = Time::getCurrentTime().formatted("%Y%m%d_%H%M%S");
        String safeName = currentSessionName.isNotEmpty() ? currentSessionName : "session";
        safeName = safeName.replaceCharacters(" /\\:*?\"<>|", "___________");
        String filename = safeName + "_" + timestamp + ".flac";
        
        // Get the default recording directory from processor
        auto parentDirUrl = processor.getDefaultRecordingDirectory();
        
        if (parentDirUrl.isEmpty())
        {
            DBG("ActiveSessionView: No recording directory set");
            return;
        }
        
        URL returl;
        bool started = processor.startRecordingToFile(parentDirUrl, filename, returl);
        
        if (started)
        {
            isRecording = true;
            recordingStartTime = Time::getMillisecondCounterHiRes();
            
            // Store the returned URL as the file path
            if (returl.isLocalFile())
                recordedFilePath = returl.getLocalFile().getFullPathName();
            else
                recordedFilePath = returl.toString(false);
            
            // Update UI
            recordButton.setButtonText("Stop");
            recordButton.setColour(TextButton::buttonColourId, Colour(0xffe74c3c));
            recordingTimerLabel.setText("00:00", dontSendNotification);
            recordingTimerLabel.setVisible(true);
            
            DBG("ActiveSessionView: Recording started to " + recordedFilePath);
        }
        else
        {
            DBG("ActiveSessionView: Failed to start recording");
            recordedFilePath = "";
        }
    }
    else
    {
        // Stop recording
        DBG("ActiveSessionView: Stopping recording...");
        
        processor.stopRecordingToFile();
        
        isRecording = false;
        recordingDuration = (Time::getMillisecondCounterHiRes() - recordingStartTime) / 1000.0;
        
        // Update UI
        recordButton.setButtonText("Record");
        recordButton.setColour(TextButton::buttonColourId, Colour(0xff6c5ce7));
        // Keep timer visible showing final duration
        recordingTimerLabel.setText(formatDuration(recordingDuration), dontSendNotification);
        
        DBG("ActiveSessionView: Recording stopped. Duration: " + String(recordingDuration) + "s");
    }
    
    if (onRecordClicked)
        onRecordClicked();
}


void ActiveSessionView::changeListenerCallback(ChangeBroadcaster* source)
{
    if (source == sessionManager)
    {
        if (sessionManager->isConnected())
        {
            currentSessionId = sessionManager->getCurrentSessionId();
            setSessionInfo(sessionManager->getCurrentSessionName(),
                           sessionManager->getInviteUrl());
            
            startTimer(recordingTimerIntervalMs);
            lastParticipantPollTime = Time::getMillisecondCounterHiRes();
            
            fetchAndUpdateParticipants();
            statusLabel.setText("Connected", dontSendNotification);
            statusLabel.setColour(Label::textColourId, Colours::green);
        }
        else
        {
            stopTimer();
            statusLabel.setText("Disconnected", dontSendNotification);
            statusLabel.setColour(Label::textColourId, Colour(0xffe74c3c));
        }
    }
}

void ActiveSessionView::fetchAndUpdateParticipants()
{
    DBG("=== fetchAndUpdateParticipants called ===");
    DBG("API pointer: " + String(api == nullptr ? "NULL" : "valid"));
    DBG("Session ID: " + currentSessionId);

    if (!api || currentSessionId.isEmpty())
    {
        updateParticipantsUI();
        return;
    }
    
    SoundFlipAPI::CollabSession session = api->getCollabSession(currentSessionId);
    
    DBG("API Status Code: " + String(api->getLastStatusCode()));
    DBG("Participants count from API: " + String(session.participants.size()));

    if (api->getLastStatusCode() != 200)
    {
        DBG("Failed to fetch session participants: " + api->getLastError());
        updateParticipantsUI();
        return;
    }
    
    if (session.participants.isEmpty())
    {
        participantListLabel.setText("Just you", dontSendNotification);
        return;
    }
    
    String participantText = "(" + String(session.participants.size()) + ") ";
    for (int i = 0; i < session.participants.size(); ++i)
    {
        if (i > 0) participantText += ", ";
        participantText += "@" + session.participants[i].username;
    }
    
    participantListLabel.setText(participantText, dontSendNotification);
}

void ActiveSessionView::updateParticipantsUI()
{
    if (!sessionManager)
    {
        participantListLabel.setText("No session", dontSendNotification);
        return;
    }
    
    const auto& participants = sessionManager->getParticipants();
    
    if (participants.isEmpty())
    {
        participantListLabel.setText("Just you", dontSendNotification);
        return;
    }
    
    String participantText = "(" + String(participants.size()) + ") ";
    for (int i = 0; i < participants.size(); ++i)
    {
        if (i > 0) participantText += ", ";
        participantText += "@" + participants[i].username;
    }
    
    participantListLabel.setText(participantText, dontSendNotification);
}

void ActiveSessionView::handleInviteClicked()
{
    if (currentInviteUrl.isNotEmpty())
    {
        SystemClipboard::copyTextToClipboard(currentInviteUrl);
        
        inviteButton.setButtonText("Copied!");
        
        Timer::callAfterDelay(2000, [this]() {
            if (inviteButton.isShowing())
                inviteButton.setButtonText("Invite");
        });
    }
    
    if (onInviteClicked)
        onInviteClicked();
}

void ActiveSessionView::handleEndSession()
{
    // Stop recording if in progress
    if (isRecording && editor)
    {
        DBG("ActiveSessionView: Stopping recording before ending session...");
        
        auto* processorPtr = dynamic_cast<SonobusAudioProcessor*>(editor->getAudioProcessor());
        if (processorPtr)
        {
            processorPtr->stopRecordingToFile();
        }
        
        isRecording = false;
        recordingDuration = (Time::getMillisecondCounterHiRes() - recordingStartTime) / 1000.0;
        recordingTimerLabel.setText(formatDuration(recordingDuration), dontSendNotification);
        DBG("ActiveSessionView: Recording stopped. Duration: " + String(recordingDuration) + "s");
    }
    
    stopTimer();
    
    // IMPORTANT: Capture the session ID NOW, before disconnecting/leaving
    if (sessionManager)
    {
        endingSessionId = sessionManager->getCurrentSessionId();
        DBG("ActiveSessionView: Captured session ID for upload: " + endingSessionId);
    }
    
    if (onEndClicked)
        onEndClicked();
    
    if (editor)
    {
        editor->disconnectSoundFlipSession();
    }
    
    if (sessionManager)
    {
        sessionManager->leaveSession();
    }
    
    if (onSessionEnded)
        onSessionEnded();
}

void ActiveSessionView::paint(Graphics& g)
{
    g.fillAll(Colour(0xff1a1a2e));
}

void ActiveSessionView::resized()
{
    auto bounds = getLocalBounds().reduced(40);
    
    titleLabel.setBounds(bounds.removeFromTop(40));
    bounds.removeFromTop(10);
    statusLabel.setBounds(bounds.removeFromTop(25));
    
    bounds.removeFromTop(20);
    
    participantsLabel.setBounds(bounds.removeFromTop(20));
    participantListLabel.setBounds(bounds.removeFromTop(25));
    
    bounds.removeFromTop(15);
    
    // Recording timer (above buttons)
    recordingTimerLabel.setBounds(bounds.removeFromTop(30));
    
    bounds.removeFromTop(15);
    
    int buttonWidth = 120;
    int buttonHeight = 40;
    int spacing = 15;
    
    auto buttonRow = bounds.removeFromTop(buttonHeight);
    int totalButtonWidth = buttonWidth * 3 + spacing * 2;
    int startX = (buttonRow.getWidth() - totalButtonWidth) / 2;
    
    recordButton.setBounds(buttonRow.getX() + startX, buttonRow.getY(), buttonWidth, buttonHeight);
    chatButton.setBounds(buttonRow.getX() + startX + buttonWidth + spacing, buttonRow.getY(), buttonWidth, buttonHeight);
    inviteButton.setBounds(buttonRow.getX() + startX + (buttonWidth + spacing) * 2, buttonRow.getY(), buttonWidth, buttonHeight);
    
    bounds.removeFromTop(30);
    
    endSessionButton.setBounds((getWidth() - 150) / 2, bounds.getY(), 150, buttonHeight);
}