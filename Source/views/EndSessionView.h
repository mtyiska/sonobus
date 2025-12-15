// SPDX-License-Identifier: GPLv3-or-later WITH Appstore-exception
// Copyright (C) 2024 SoundFlip

#pragma once

#include <JuceHeader.h>

class SessionManager;

class EndSessionView : public Component, public Button::Listener
{
public:
    EndSessionView();
    ~EndSessionView() override;
    
    void setSessionManager(SessionManager* sm);
    void setSessionId(const String& sessionId);
    void setRecordingInfo(const String& filePath, double durationSeconds);
    void resetState();
    
    // Callbacks
    std::function<void()> onUploadClicked;
    std::function<void()> onSaveLocallyClicked;
    std::function<void()> onDiscardClicked;
    std::function<void()> onComplete;
    
    // Component overrides
    void paint(Graphics& g) override;
    void resized() override;
    
    // Button::Listener
    void buttonClicked(Button* buttonThatWasClicked) override;
    
private:
    void handleUpload();
    void handleSaveLocally();
    void handleDiscard();
    
    String formatDuration(double seconds) const;
    void setButtonsEnabled(bool enabled);
    void showStatus(const String& message, Colour color);
    
    SessionManager* sessionManager = nullptr;
    String currentSessionId;
    String recordedFilePath;
    double recordingDuration = 0.0;
    bool isUploading = false;
    
    std::unique_ptr<Label> titleLabel;
    std::unique_ptr<Label> recordingInfoLabel;
    std::unique_ptr<Label> statusLabel;
    std::unique_ptr<TextButton> uploadButton;
    std::unique_ptr<TextButton> saveLocallyButton;
    std::unique_ptr<TextButton> discardButton;
    std::unique_ptr<TextButton> doneButton;
    
    std::unique_ptr<FileChooser> fileChooser;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EndSessionView)
};