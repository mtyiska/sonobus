#pragma once

#include <JuceHeader.h>
#include "../SonobusPluginProcessor.h"

class SessionManager;
class SonobusAudioProcessorEditor;
class SoundFlipAPI;

class ActiveSessionView : public Component,
                          public ChangeListener,
                          public Timer
{
public:
    ActiveSessionView();
    
    ActiveSessionView(SessionManager* sessionManager, 
                      SonobusAudioProcessorEditor* editor);
    
    ActiveSessionView(SessionManager* sessionManager, 
                      SonobusAudioProcessorEditor* editor,
                      SoundFlipAPI* api);
    
    ~ActiveSessionView() override;

    void paint(Graphics& g) override;
    void resized() override;
    void timerCallback() override;
    
    void setSessionManager(SessionManager* sm);
    void setSoundFlipAPI(SoundFlipAPI* api);
    void setSessionInfo(const String& name, const String& inviteUrl);
    void refreshParticipants();
    
    void changeListenerCallback(ChangeBroadcaster* source) override;
    
    // Recording state accessors for EndSessionView
    String getRecordedFilePath() const { return recordedFilePath; }
    double getRecordingDuration() const { return recordingDuration; }
    bool hasRecording() const { return recordedFilePath.isNotEmpty(); }
    void clearRecordingInfo();
    String getEndingSessionId() const { return endingSessionId; }

    std::function<void()> onEndClicked;
    std::function<void()> onRecordClicked;
    std::function<void()> onChatClicked;
    std::function<void()> onInviteClicked;
    std::function<void()> onSessionEnded;

private:
    void setupUI();
    void handleEndSession();
    void handleInviteClicked();
    void handleRecordClicked();
    void updateParticipantsUI();
    void fetchAndUpdateParticipants();
    void updateRecordingTimerDisplay();
    String formatDuration(double seconds) const;

    SessionManager* sessionManager = nullptr;
    SonobusAudioProcessorEditor* editor = nullptr;
    SoundFlipAPI* api = nullptr;
    
    String currentSessionId;
    static constexpr int pollIntervalMs = 60000;  // 60 seconds for participant polling
    static constexpr int recordingTimerIntervalMs = 100;  // 100ms for smooth timer display

    // Recording state
    bool isRecording = false;
    double recordingStartTime = 0.0;
    double recordingDuration = 0.0;
    String recordedFilePath;
    String endingSessionId; 

    Label titleLabel;
    Label statusLabel;
    Label participantsLabel;
    Label participantListLabel;
    Label recordingTimerLabel;
    TextButton recordButton;
    TextButton chatButton;
    TextButton inviteButton;
    TextButton endSessionButton;
    
    String currentSessionName;
    String currentInviteUrl;
    
    // Track last participant poll time
    double lastParticipantPollTime = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ActiveSessionView)
};