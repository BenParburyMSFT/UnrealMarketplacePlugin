//////////////////////////////////////////////////////
// Copyright (C) Microsoft. 2018. All rights reserved.
//////////////////////////////////////////////////////


// This is automatically generated by PlayFab SDKGenerator. DO NOT modify this manually!
#include "Core/PlayFabAuthenticationAPI.h"
#include "Core/PlayFabSettings.h"
#include "Core/PlayFabResultHandler.h"
#include "PlayFab.h"

using namespace PlayFab;
using namespace PlayFab::AuthenticationModels;

UPlayFabAuthenticationAPI::UPlayFabAuthenticationAPI() {}

UPlayFabAuthenticationAPI::~UPlayFabAuthenticationAPI() {}

int UPlayFabAuthenticationAPI::GetPendingCalls() const
{
    return PlayFabRequestHandler::GetPendingCalls();
}

FString UPlayFabAuthenticationAPI::GetBuildIdentifier() const
{
    return PlayFabSettings::buildIdentifier;
}

void UPlayFabAuthenticationAPI::SetTitleId(const FString& titleId)
{
    PlayFabSettings::SetTitleId(titleId);
}

void UPlayFabAuthenticationAPI::SetDevSecretKey(const FString& developerSecretKey)
{
    PlayFabSettings::SetDeveloperSecretKey(developerSecretKey);
}

bool UPlayFabAuthenticationAPI::GetEntityToken(
    AuthenticationModels::FGetEntityTokenRequest& request,
    const FGetEntityTokenDelegate& SuccessDelegate,
    const FPlayFabErrorDelegate& ErrorDelegate)
{
    FString authKey; FString authValue;
    if (request.AuthenticationContext.IsValid()) {
        if (request.AuthenticationContext->GetEntityToken().Len() > 0) {
            authKey = TEXT("X-EntityToken"); authValue = request.AuthenticationContext->GetEntityToken();
        } else if (request.AuthenticationContext->GetClientSessionTicket().Len() > 0) {
            authKey = TEXT("X-Authorization"); authValue = request.AuthenticationContext->GetClientSessionTicket();
        } else if (request.AuthenticationContext->GetDeveloperSecretKey().Len() > 0) {
            authKey = TEXT("X-SecretKey"); authValue = request.AuthenticationContext->GetDeveloperSecretKey();
        }
    }
    else {
        if (PlayFabSettings::GetEntityToken().Len() > 0) {
            authKey = TEXT("X-EntityToken"); authValue = PlayFabSettings::GetEntityToken();
        } else if (PlayFabSettings::GetClientSessionTicket().Len() > 0) {
            authKey = TEXT("X-Authorization"); authValue = PlayFabSettings::GetClientSessionTicket();
        } else if (PlayFabSettings::GetDeveloperSecretKey().Len() > 0) {
            authKey = TEXT("X-SecretKey"); authValue = PlayFabSettings::GetDeveloperSecretKey();
        }
    }


    auto HttpRequest = PlayFabRequestHandler::SendRequest(PlayFabSettings::GetUrl(TEXT("/Authentication/GetEntityToken")), request.toJSONString(), authKey, authValue);
    HttpRequest->OnProcessRequestComplete().BindRaw(this, &UPlayFabAuthenticationAPI::OnGetEntityTokenResult, SuccessDelegate, ErrorDelegate);
    return HttpRequest->ProcessRequest();
}

void UPlayFabAuthenticationAPI::OnGetEntityTokenResult(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded, FGetEntityTokenDelegate SuccessDelegate, FPlayFabErrorDelegate ErrorDelegate)
{
    AuthenticationModels::FGetEntityTokenResponse outResult;
    FPlayFabCppError errorResult;
    if (PlayFabRequestHandler::DecodeRequest(HttpRequest, HttpResponse, bSucceeded, outResult, errorResult))
    {
        if (outResult.EntityToken.Len() > 0)
            PlayFabSettings::SetEntityToken(outResult.EntityToken);
        SuccessDelegate.ExecuteIfBound(outResult);
    }
    else
    {
        ErrorDelegate.ExecuteIfBound(errorResult);
    }
}

bool UPlayFabAuthenticationAPI::ValidateEntityToken(
    AuthenticationModels::FValidateEntityTokenRequest& request,
    const FValidateEntityTokenDelegate& SuccessDelegate,
    const FPlayFabErrorDelegate& ErrorDelegate)
{
    if ((request.AuthenticationContext.IsValid() && request.AuthenticationContext->GetEntityToken().Len() == 0)
        || (!request.AuthenticationContext.IsValid() && PlayFabSettings::GetEntityToken().Len() == 0)) {
        UE_LOG(LogPlayFabCpp, Error, TEXT("You must call GetEntityToken API Method before calling this function."));
    }


    auto HttpRequest = PlayFabRequestHandler::SendRequest(PlayFabSettings::GetUrl(TEXT("/Authentication/ValidateEntityToken")), request.toJSONString(), TEXT("X-EntityToken"), !request.AuthenticationContext.IsValid() ? PlayFabSettings::GetEntityToken() : request.AuthenticationContext->GetEntityToken());
    HttpRequest->OnProcessRequestComplete().BindRaw(this, &UPlayFabAuthenticationAPI::OnValidateEntityTokenResult, SuccessDelegate, ErrorDelegate);
    return HttpRequest->ProcessRequest();
}

void UPlayFabAuthenticationAPI::OnValidateEntityTokenResult(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded, FValidateEntityTokenDelegate SuccessDelegate, FPlayFabErrorDelegate ErrorDelegate)
{
    AuthenticationModels::FValidateEntityTokenResponse outResult;
    FPlayFabCppError errorResult;
    if (PlayFabRequestHandler::DecodeRequest(HttpRequest, HttpResponse, bSucceeded, outResult, errorResult))
    {
        SuccessDelegate.ExecuteIfBound(outResult);
    }
    else
    {
        ErrorDelegate.ExecuteIfBound(errorResult);
    }
}
