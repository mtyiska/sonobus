// SPDX-License-Identifier: GPLv3-or-later WITH Appstore-exception
// Copyright (C) 2024 SoundFlip

#include "EndSessionView.h"
#include "../managers/SessionManager.h"

EndSessionView::EndSessionView()
{
    titleLabel = std::make_unique<Label>("title", "Session ended");
    titleLabel->setFont(Font(28.0f, Font::bold));
    titleLabel->setColour(Label::textColourId, Colours::white);
    titleLabel->setJustificationType(Justification::centred);
    addAndMakeVisible(titleLabel.get());

    recordingInfoLabel = std::make_unique<Label>("info", "No recording");
    recordingInfoLabel->setFont(Font(16.0f));
    recordingInfoLabel->setColour(Label::textColourId, Colours::grey);
    recordingInfoLabel->setJustificationType(Justification::centred);
    addAndMakeVisible(recordingInfoLabel.get());
    
    statusLabel = std::make_unique<Label>("status", "");
    statusLabel->setFont(Font(14.0f));
    statusLabel->setColour(Label::textColourId, Colour(0xff00cec9));
    statusLabel->setJustificationType(Justification::centred);
    statusLabel->setVisible(false);
    addAndMakeVisible(statusLabel.get());

    uploadButton = std::make_unique<TextButton>("Upload to this session");
    uploadButton->addListener(this);
    uploadButton->setColour(TextButton::buttonColourId, Colour(0xff6c5ce7));
    addAndMakeVisible(uploadButton.get());

    saveLocallyButton = std::make_unique<TextButton>("Save locally only");
    saveLocallyButton->addListener(this);
    addAndMakeVisible(saveLocallyButton.get());

    discardButton = std::make_unique<TextButton>("Discard recording");
    discardButton->addListener(this);
    discardButton->setColour(TextButton::buttonColourId, Colour(0xff555555));
    addAndMakeVisible(discardButton.get());
    
    doneButton = std::make_unique<TextButton>("Done");
    doneButton->addListener(this);
    doneButton->setColour(TextButton::buttonColourId, Colour(0xff00cec9));
    doneButton->setVisible(false);
    addAndMakeVisible(doneButton.get());
}

EndSessionView::~EndSessionView()
{
}

void EndSessionView::setSessionManager(SessionManager* sm)
{
    sessionManager = sm;
}

void EndSessionView::setSessionId(const String& sessionId)
{
    currentSessionId = sessionId;
}

void EndSessionView::setRecordingInfo(const String& filePath, double durationSeconds)
{
    recordedFilePath = filePath;
    recordingDuration = durationSeconds;
    
    if (filePath.isEmpty() || durationSeconds <= 0)
    {
        recordingInfoLabel->setText("No recording", dontSendNotification);
        uploadButton->setEnabled(false);
        saveLocallyButton->setEnabled(false);
        discardButton->setEnabled(false);
        
        // Show only done button when no recording
        uploadButton->setVisible(false);
        saveLocallyButton->setVisible(false);
        discardButton->setVisible(false);
        doneButton->setVisible(true);
    }
    else
    {
        String durationStr = formatDuration(durationSeconds);
        recordingInfoLabel->setText("You recorded " + durationStr + " of audio", dontSendNotification);
        uploadButton->setEnabled(true);
        saveLocallyButton->setEnabled(true);
        discardButton->setEnabled(true);
        
        uploadButton->setVisible(true);
        saveLocallyButton->setVisible(true);
        discardButton->setVisible(true);
        doneButton->setVisible(false);
    }
}

void EndSessionView::resetState()
{
    isUploading = false;
    statusLabel->setVisible(false);
    setButtonsEnabled(true);
    
    // Reset to default state
    uploadButton->setVisible(true);
    saveLocallyButton->setVisible(true);
    discardButton->setVisible(true);
    doneButton->setVisible(false);
}

String EndSessionView::formatDuration(double seconds) const
{
    int totalSeconds = static_cast<int>(seconds);
    int minutes = totalSeconds / 60;
    int secs = totalSeconds % 60;
    
    if (minutes > 0)
        return String(minutes) + ":" + String::formatted("%02d", secs);
    else
        return String(secs) + " seconds";
}

void EndSessionView::setButtonsEnabled(bool enabled)
{
    uploadButton->setEnabled(enabled);
    saveLocallyButton->setEnabled(enabled);
    discardButton->setEnabled(enabled);
}

void EndSessionView::showStatus(const String& message, Colour color)
{
    statusLabel->setText(message, dontSendNotification);
    statusLabel->setColour(Label::textColourId, color);
    statusLabel->setVisible(true);
}

void EndSessionView::paint(Graphics& g)
{
    g.fillAll(Colour(0xff1a1a2e));
}

void EndSessionView::resized()
{
    auto bounds = getLocalBounds();
    auto centerX = bounds.getCentreX();
    auto centerY = bounds.getCentreY();

    titleLabel->setBounds(centerX - 150, centerY - 140, 300, 35);
    recordingInfoLabel->setBounds(centerX - 150, centerY - 95, 300, 25);
    statusLabel->setBounds(centerX - 150, centerY - 65, 300, 25);

    uploadButton->setBounds(centerX - 120, centerY - 30, 240, 45);
    saveLocallyButton->setBounds(centerX - 120, centerY + 25, 240, 45);
    discardButton->setBounds(centerX - 120, centerY + 80, 240, 45);
    doneButton->setBounds(centerX - 120, centerY - 30, 240, 45);
}

void EndSessionView::buttonClicked(Button* buttonThatWasClicked)
{
    if (buttonThatWasClicked == uploadButton.get())
    {
        handleUpload();
    }
    else if (buttonThatWasClicked == saveLocallyButton.get())
    {
        handleSaveLocally();
    }
    else if (buttonThatWasClicked == discardButton.get())
    {
        handleDiscard();
    }
    else if (buttonThatWasClicked == doneButton.get())
    {
        if (onComplete)
            onComplete();
    }
}

void EndSessionView::handleUpload()
{
    if (recordedFilePath.isEmpty() || !sessionManager)
    {
        showStatus("Cannot upload: missing file or session", Colour(0xffe74c3c));
        return;
    }
    
    File recordedFile(recordedFilePath);
    if (!recordedFile.existsAsFile())
    {
        showStatus("Recording file not found", Colour(0xffe74c3c));
        return;
    }
    
    isUploading = true;
    setButtonsEnabled(false);
    showStatus("Uploading...", Colour(0xff00cec9));
    
    // Perform upload on background thread
    Thread::launch([this, recordedFile]() {
        bool success = false;
        String errorMessage;
        
        // Call SessionManager's upload method
        if (sessionManager)
        {
            success = sessionManager->uploadStem(currentSessionId, recordedFile, 
                [this](float progress) {
                    // Update progress on message thread
                    MessageManager::callAsync([this, progress]() {
                        int percent = static_cast<int>(progress * 100);
                        showStatus("Uploading... " + String(percent) + "%", Colour(0xff00cec9));
                    });
                },
                errorMessage);
        }
        
        // Update UI on message thread
        MessageManager::callAsync([this, success, errorMessage]() {
            isUploading = false;
            
            if (success)
            {
                showStatus("Upload complete!", Colours::green);
                
                // Show done button, hide others
                uploadButton->setVisible(false);
                saveLocallyButton->setVisible(false);
                discardButton->setVisible(false);
                doneButton->setVisible(true);
                
                if (onUploadClicked)
                    onUploadClicked();
            }
            else
            {
                showStatus("Upload failed: " + errorMessage, Colour(0xffe74c3c));
                setButtonsEnabled(true);
            }
        });
    });
}

void EndSessionView::handleSaveLocally()
{
    if (recordedFilePath.isEmpty())
    {
        showStatus("No recording to save", Colour(0xffe74c3c));
        return;
    }
    
    File sourceFile(recordedFilePath);
    if (!sourceFile.existsAsFile())
    {
        showStatus("Recording file not found", Colour(0xffe74c3c));
        return;
    }
    
    // Use async file chooser (JUCE 7+ style)
    fileChooser = std::make_unique<FileChooser>(
        "Save Recording As...",
        File::getSpecialLocation(File::userMusicDirectory).getChildFile(sourceFile.getFileName()),
        "*.flac;*.wav",
        true);
    
    auto chooserFlags = FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles;
    
    fileChooser->launchAsync(chooserFlags, [this, sourceFile](const FileChooser& fc)
    {
        auto result = fc.getResult();
        if (result == File())
        {
            // User cancelled
            return;
        }
        
        File destFile = result;
        
        // Copy the file
        if (sourceFile.copyFileTo(destFile))
        {
            showStatus("Saved to: " + destFile.getFileName(), Colours::green);
            
            // Hide action buttons, show done button
            uploadButton->setVisible(false);
            saveLocallyButton->setVisible(false);
            discardButton->setVisible(false);
            doneButton->setVisible(true);
            
            if (onSaveLocallyClicked)
                onSaveLocallyClicked();
        }
        else
        {
            showStatus("Failed to save file", Colour(0xffe74c3c));
        }
    });
}

void EndSessionView::handleDiscard()
{
    if (recordedFilePath.isEmpty())
    {
        // Nothing to discard, just show done
        uploadButton->setVisible(false);
        saveLocallyButton->setVisible(false);
        discardButton->setVisible(false);
        doneButton->setVisible(true);
        return;
    }
    
    // Use async alert (JUCE 7+ style)
    auto options = MessageBoxOptions()
        .withIconType(MessageBoxIconType::WarningIcon)
        .withTitle("Discard Recording")
        .withMessage("Are you sure you want to discard this recording? This cannot be undone.")
        .withButton("Discard")
        .withButton("Cancel")
        .withAssociatedComponent(this);
    
    AlertWindow::showAsync(options, [this](int result)
    {
        if (result == 1) // "Discard" button (first button = 1)
        {
            // Delete the file
            File recordingFile(recordedFilePath);
            if (recordingFile.existsAsFile())
            {
                recordingFile.deleteFile();
                DBG("EndSessionView: Deleted recording file");
            }
            
            recordedFilePath = "";
            showStatus("Recording discarded", Colours::grey);
            
            // Hide action buttons, show done button
            uploadButton->setVisible(false);
            saveLocallyButton->setVisible(false);
            discardButton->setVisible(false);
            doneButton->setVisible(true);
            
            if (onDiscardClicked)
                onDiscardClicked();
        }
        // If result == 0 (Cancel), do nothing
    });
}