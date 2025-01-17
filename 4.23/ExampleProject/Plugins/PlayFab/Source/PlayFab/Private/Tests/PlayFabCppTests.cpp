//////////////////////////////////////////////////////
// Copyright (C) Microsoft. 2018. All rights reserved.
//////////////////////////////////////////////////////


#include "Tests/PlayFabCppTests.h"

#include "IPlayFab.h"
#include "PlayFabPrivate.h"
#include "Core/PlayFabClientAPI.h"
#include "Core/PlayFabServerAPI.h"
#include "Core/PlayFabDataAPI.h"
#include "Core/PlayFabAuthenticationAPI.h"
#include "TestFramework/PlayFabTestRunner.h"

void UPlayFabCppTests::SetTestTitleData(const UTestTitleDataLoader& testTitleData)
{
    UserEmail = testTitleData.userEmail;
}

void UPlayFabCppTests::ClassSetUp()
{
    ClientAPI = IPlayFabModuleInterface::Get().GetClientAPI();
    ServerAPI = IPlayFabModuleInterface::Get().GetServerAPI();
    DataAPI = IPlayFabModuleInterface::Get().GetDataAPI();
    AuthenticationAPI = IPlayFabModuleInterface::Get().GetAuthenticationAPI();
}

void UPlayFabCppTests::SetUp(UPlayFabTestContext* testContext)
{
    CurrentTestContext = testContext;

    if (ServerAPI == nullptr || !ServerAPI.IsValid())
    {
        testContext->EndTest(PlayFabApiTestFinishState::SKIPPED, "Could not load the PlayFab API");
    }

    FString titleId = GetDefault<UPlayFabRuntimeSettings>()->TitleId;
    if (titleId.Len() == 0)
    {
        testContext->EndTest(PlayFabApiTestFinishState::SKIPPED, "Could not load testTitleData.json");
    }
}

void UPlayFabCppTests::OnSharedError(const PlayFab::FPlayFabCppError& InError) const
{
    UE_LOG(LogPlayFabTests, Error, TEXT("%s encountered an error:\n%s"), *CurrentTestContext->testName, *InError.
        GenerateErrorReport());

    CurrentTestContext->EndTest(PlayFabApiTestFinishState::FAILED, InError.ErrorMessage);
}


void UPlayFabCppTests::InvalidLogin()
{
    UE_LOG(LogPlayFabTests, Log, TEXT("- INVALID LOGIN -"));

    PlayFab::ClientModels::FLoginWithEmailAddressRequest request;
    request.Email = UserEmail;
    request.Password = TEXT("INVALID");

    ClientAPI->LoginWithEmailAddress(request,
                                     PlayFab::UPlayFabClientAPI::FLoginWithEmailAddressDelegate::CreateUObject(
                                         this, &UPlayFabCppTests::InvalidLogin_Success),
                                     PlayFab::FPlayFabErrorDelegate::CreateUObject(
                                         this, &UPlayFabCppTests::InvalidLogin_Error)
    );
}

void UPlayFabCppTests::InvalidLogin_Success(const PlayFab::ClientModels::FLoginResult& result)
{
    CurrentTestContext->EndTest(PlayFabApiTestFinishState::FAILED, TEXT("Login Succeeded where it should have failed."));
}

void UPlayFabCppTests::InvalidLogin_Error(const PlayFab::FPlayFabCppError& InErrorResult)
{
    if (InErrorResult.ErrorMessage.Find(TEXT("password")) == -1)
        // Check that we correctly received a notice about invalid password
    {
        UE_LOG(LogPlayFabTests, Error, TEXT("Non-password error with login"));
        CurrentTestContext->EndTest(PlayFabApiTestFinishState::FAILED);
    }

    CurrentTestContext->EndTest();
}


void UPlayFabCppTests::LoginOrRegister()
{
    PlayFab::ClientModels::FLoginWithCustomIDRequest Request;
    Request.CustomId = ClientAPI->GetBuildIdentifier();
    Request.CreateAccount = true;

    ClientAPI->LoginWithCustomID(
        Request,
        PlayFab::UPlayFabClientAPI::FLoginWithCustomIDDelegate::CreateUObject(this, &UPlayFabCppTests::LoginOrRegister_Success),
        PlayFab::FPlayFabErrorDelegate::CreateUObject(this, &UPlayFabCppTests::OnSharedError)
    );
}

void UPlayFabCppTests::LoginOrRegister_Success(const PlayFab::ClientModels::FLoginResult& result)
{
    PlayFabId = result.PlayFabId;
    UE_LOG(LogPlayFabTests, Log, TEXT("PlayFab login successful: %s, %s"), *PlayFabId, *ClientAPI->GetBuildIdentifier());

    CurrentTestContext->EndTest();
}

void UPlayFabCppTests::CheckIfLoggedIn()
{
    IPlayFab* playFabSettings = &(IPlayFab::Get());
    if (!playFabSettings->IsClientLoggedIn())
    {
        CurrentTestContext->EndTest(PlayFabApiTestFinishState::SKIPPED, "Earlier tests failed to log in");
    }
}

void UPlayFabCppTests::UserDataAPI()
{
    CheckIfLoggedIn();

    PlayFab::ClientModels::FGetUserDataRequest Request;
    ClientAPI->GetUserData(
        Request,
        PlayFab::UPlayFabClientAPI::FGetUserDataDelegate::CreateUObject(this, &UPlayFabCppTests::GetUserDataAPI_Success),
        PlayFab::FPlayFabErrorDelegate::CreateUObject(this, &UPlayFabCppTests::OnSharedError)
    );
}

void UPlayFabCppTests::GetUserDataAPI_Success(const PlayFab::ClientModels::FGetUserDataResult& result)
{
    int actualValue = -1;

    const PlayFab::ClientModels::FUserDataRecord* target = result.Data.Find(TEST_DATA_KEY);
    if (target != nullptr)
        actualValue = FCString::Atoi(*(target->Value));

    if (UserDataAPI_ExpectedValue != -1)
    {
        if (UserDataAPI_ExpectedValue != actualValue)
        {
            // If I know what value I'm expecting, and I did not get it, log an error
            UE_LOG(LogPlayFabTests, Error, TEXT("GetUserData: Update value did not match new value %d!=%d"), UserDataAPI_ExpectedValue, actualValue);

            CurrentTestContext->EndTest(PlayFabApiTestFinishState::FAILED, FString::Format(TEXT("Update value did not match new value {0}!={1}"), { UserDataAPI_ExpectedValue, actualValue })); // Error
        }
        else
        {
            // If I know what value I'm expecting, and I got it, test passed, exit
            //CheckTimestamp(target->LastUpdated); // If the value was updated correctly, check the timestamp
            {
                const auto updateTime = target->LastUpdated;
                FDateTime utcNow = FDateTime::UtcNow();
                FTimespan delta = FTimespan(0, 5, 0);
                FDateTime minTest = utcNow - delta;
                FDateTime maxTest = utcNow + delta;

                if (minTest <= updateTime && updateTime <= maxTest)
                {
                    UE_LOG(LogPlayFabTests, Log, TEXT("GetUserData: LastUpdated timestamp parsed as expected"));
                    CurrentTestContext->EndTest();
                }
                else
                {
                    UE_LOG(LogPlayFabTests, Error, TEXT("GetUserData: LastUpdated timestamp was not parsed correctly"));
                    CurrentTestContext->EndTest(PlayFabApiTestFinishState::FAILED, TEXT("LastUpdated timestamp was not parsed correctly"));
                }
            }

            UE_LOG(LogPlayFabTests, Log, TEXT("GetUserData Success"));
            CurrentTestContext->EndTest();
        }
    }
    else
    {
        // If I don't know what value I was expecting, Call Update with (actualValue + 1)
        actualValue = (actualValue + 1) % 100;

        FString strUpdateValue;
        strUpdateValue.AppendInt(actualValue);

        PlayFab::ClientModels::FUpdateUserDataRequest Request;
        Request.Data.Add(TEST_DATA_KEY, strUpdateValue);

        UserDataAPI_ExpectedValue = actualValue; // Update that for next Get.

        ClientAPI->UpdateUserData(
            Request,
            PlayFab::UPlayFabClientAPI::FUpdateUserDataDelegate::CreateUObject(this, &UPlayFabCppTests::UpdateUserDataAPI_Success),
            PlayFab::FPlayFabErrorDelegate::CreateUObject(this, &UPlayFabCppTests::OnSharedError)
        );
    }
}

void UPlayFabCppTests::UpdateUserDataAPI_Success(const PlayFab::ClientModels::FUpdateUserDataResult& result)
{
    UserDataAPI(); // Restart from the GET to check updated values.
}


void UPlayFabCppTests::PlayerStatisticsAPI()
{
    CheckIfLoggedIn();

    PlayFab::ClientModels::FGetPlayerStatisticsRequest Request;
    ClientAPI->GetPlayerStatistics(
        Request,
        PlayFab::UPlayFabClientAPI::FGetPlayerStatisticsDelegate::CreateUObject(this, &UPlayFabCppTests::GetPlayerStatisticsAPI_Success),
        PlayFab::FPlayFabErrorDelegate::CreateUObject(this, &UPlayFabCppTests::OnSharedError)
    );
}

void UPlayFabCppTests::GetPlayerStatisticsAPI_Success(const PlayFab::ClientModels::FGetPlayerStatisticsResult& result)
{
    int actualValue = -1000;
    for (int i = 0; i < result.Statistics.Num(); i++)
        if (result.Statistics[i].StatisticName == TEST_STAT_NAME)
            actualValue = result.Statistics[i].Value;

    if (PlayerStatistics_ExpectedValue != -1)
    {
        if (PlayerStatistics_ExpectedValue != actualValue)
        {
            const FString errMsg = TEXT("Update value did not match new value");

            UE_LOG(LogPlayFabTests, Error, TEXT("GetPlayerStatistics: %s"), *errMsg);
            CurrentTestContext->EndTest(PlayFabApiTestFinishState::FAILED, errMsg); // ERROR
        }
        else
        {
            UE_LOG(LogPlayFabTests, Log, TEXT("GetPlayerStatistics Success"));
            CurrentTestContext->EndTest(); // SUCCESS
        }
    }
    else
    {
        // Call Update with (actualValue + 1)
        actualValue = (actualValue + 1) % 100;

        PlayFab::ClientModels::FUpdatePlayerStatisticsRequest Request;
        PlayFab::ClientModels::FStatisticUpdate StatUpdate;
        StatUpdate.StatisticName = TEST_STAT_NAME;
        StatUpdate.Value = actualValue;
        Request.Statistics.Add(StatUpdate);

        PlayerStatistics_ExpectedValue = actualValue; // Update Expected value

        ClientAPI->UpdatePlayerStatistics(
            Request,
            PlayFab::UPlayFabClientAPI::FUpdatePlayerStatisticsDelegate::CreateUObject(this, &UPlayFabCppTests::UpdatePlayerStatisticsAPI_Success),
            PlayFab::FPlayFabErrorDelegate::CreateUObject(this, &UPlayFabCppTests::OnSharedError)
        );
    }
}

void UPlayFabCppTests::UpdatePlayerStatisticsAPI_Success(
    const PlayFab::ClientModels::FUpdatePlayerStatisticsResult& result)
{
    PlayerStatisticsAPI(); // Restart with a GET
}


void UPlayFabCppTests::LeaderBoardAPIClient()
{
    CheckIfLoggedIn();

    PlayFab::ClientModels::FGetLeaderboardRequest Request;
    Request.MaxResultsCount = 3;
    Request.StatisticName = TEST_STAT_NAME;

    ClientAPI->GetLeaderboard(
        Request,
        PlayFab::UPlayFabClientAPI::FGetLeaderboardDelegate::CreateUObject(this, &UPlayFabCppTests::LeaderBoardAPIClient_Success),
        PlayFab::FPlayFabErrorDelegate::CreateUObject(this, &UPlayFabCppTests::OnSharedError)
    );
}

void UPlayFabCppTests::LeaderBoardAPIClient_Success(const PlayFab::ClientModels::FGetLeaderboardResult& result)
{
    const int count = result.Leaderboard.Num();
    if (count > 0)
    {
        UE_LOG(LogPlayFabTests, Log, TEXT("GetLeaderboard Succeeded"));
        CurrentTestContext->EndTest();
    }
    else
    {
        UE_LOG(LogPlayFabTests, Error, TEXT("GetLeaderboard found zero results."));
        CurrentTestContext->EndTest(PlayFabApiTestFinishState::FAILED, TEXT("GetLeaderboard found zero results.")); // ERROR
    }
}

void UPlayFabCppTests::LeaderBoardAPIServer()
{
    CheckIfLoggedIn();

    PlayFab::ServerModels::FGetLeaderboardRequest Request;
    Request.MaxResultsCount = 3;
    Request.StatisticName = TEST_STAT_NAME;

    ServerAPI->GetLeaderboard(
        Request,
        PlayFab::UPlayFabServerAPI::FGetLeaderboardDelegate::CreateUObject(this, &UPlayFabCppTests::LeaderBoardAPIServer_Success),
        PlayFab::FPlayFabErrorDelegate::CreateUObject(this, &UPlayFabCppTests::OnSharedError)
    );
}

void UPlayFabCppTests::LeaderBoardAPIServer_Success(const PlayFab::ServerModels::FGetLeaderboardResult& result)
{
    const int count = result.Leaderboard.Num();
    if (count > 0)
    {
        UE_LOG(LogPlayFabTests, Log, TEXT("GetLeaderboard Succeeded"));
        CurrentTestContext->EndTest();
    }
    else
    {
        UE_LOG(LogPlayFabTests, Error, TEXT("GetLeaderboard found zero results."));
        CurrentTestContext->EndTest(PlayFabApiTestFinishState::FAILED, TEXT("GetLeaderboard found zero results.")); // ERROR
    }
}


void UPlayFabCppTests::AccountInfo()
{
    CheckIfLoggedIn();

    PlayFab::ClientModels::FGetAccountInfoRequest Request;
    ClientAPI->GetAccountInfo(
        Request,
        PlayFab::UPlayFabClientAPI::FGetAccountInfoDelegate::CreateUObject(this, &UPlayFabCppTests::AccountInfo_Success),
        PlayFab::FPlayFabErrorDelegate::CreateUObject(this, &UPlayFabCppTests::OnSharedError)
    );
}

void UPlayFabCppTests::AccountInfo_Success(const PlayFab::ClientModels::FGetAccountInfoResult& result)
{
    auto origination = result.AccountInfo->TitleInfo->Origination.mValue;
    // C++ can't really do anything with this once fetched
    CurrentTestContext->EndTest();
}


void UPlayFabCppTests::CloudScript()
{
    CheckIfLoggedIn();

    PlayFab::ClientModels::FExecuteCloudScriptRequest Request;
    Request.FunctionName = CLOUD_FUNCTION_HELLO_WORLD;

    ClientAPI->ExecuteCloudScript(
        Request,
        PlayFab::UPlayFabClientAPI::FExecuteCloudScriptDelegate::CreateUObject(this, &UPlayFabCppTests::CloudScript_Success),
        PlayFab::FPlayFabErrorDelegate::CreateUObject(this, &UPlayFabCppTests::OnSharedError)
    );
}

void UPlayFabCppTests::CloudScript_Success(const PlayFab::ClientModels::FExecuteCloudScriptResult& result)
{
    CurrentTestContext->EndTest();
}


void UPlayFabCppTests::CloudScriptError()
{
    CheckIfLoggedIn();

    PlayFab::ClientModels::FExecuteCloudScriptRequest Request;
    Request.FunctionName = CLOUD_FUNCTION_THROW_ERROR;

    ClientAPI->ExecuteCloudScript(
        Request,
        PlayFab::UPlayFabClientAPI::FExecuteCloudScriptDelegate::CreateUObject(this, &UPlayFabCppTests::CloudScriptError_Success),
        PlayFab::FPlayFabErrorDelegate::CreateUObject(this, &UPlayFabCppTests::OnSharedError)
    );
}

void UPlayFabCppTests::CloudScriptError_Success(const PlayFab::ClientModels::FExecuteCloudScriptResult& result)
{
    if (result.FunctionResult.notNull())
    {
        CurrentTestContext->EndTest(PlayFabApiTestFinishState::FAILED, TEXT("FunctionResult should be null"));
        return;
    }
    if (!result.Error.IsValid())
    {
        CurrentTestContext->EndTest(PlayFabApiTestFinishState::FAILED, TEXT("CloudScript Error not found."));
        return;
    }

    if (result.Error->Error.Find("JavascriptException") == 0)
        CurrentTestContext->EndTest();
    else
        CurrentTestContext->EndTest(PlayFabApiTestFinishState::FAILED, TEXT("Can't find Cloud Script failure details."));
}


void UPlayFabCppTests::WriteEvent()
{
    CheckIfLoggedIn();

    PlayFab::ClientModels::FWriteClientPlayerEventRequest Request;
    Request.EventName = TEXT("ForumPostEvent");
    //request.Body.Add(TEXT("Subject"), "My First Post");
    //request.Body.Add(TEXT("Body"), "My awesome Post.");

    ClientAPI->WritePlayerEvent(
        Request,
        PlayFab::UPlayFabClientAPI::FWritePlayerEventDelegate::CreateUObject(this, &UPlayFabCppTests::WriteEvent_Success),
        PlayFab::FPlayFabErrorDelegate::CreateUObject(this, &UPlayFabCppTests::OnSharedError)
    );
}

void UPlayFabCppTests::WriteEvent_Success(const PlayFab::ClientModels::FWriteEventResponse& result)
{
    CurrentTestContext->EndTest();
}


void UPlayFabCppTests::GetEntityToken()
{
    CheckIfLoggedIn();

    PlayFab::AuthenticationModels::FGetEntityTokenRequest Request;
    AuthenticationAPI->GetEntityToken(
        Request,
        PlayFab::UPlayFabAuthenticationAPI::FGetEntityTokenDelegate::CreateUObject(this, &UPlayFabCppTests::GetEntityToken_Success),
        PlayFab::FPlayFabErrorDelegate::CreateUObject(this, &UPlayFabCppTests::OnSharedError)
    );
}

void UPlayFabCppTests::GetEntityToken_Success(const PlayFab::AuthenticationModels::FGetEntityTokenResponse& result)
{
    entityId = result.Entity->Id;
    entityType = result.Entity->Type;

    CurrentTestContext->EndTest();
}


void UPlayFabCppTests::ObjectAPI()
{
    CheckIfLoggedIn();

    PlayFab::DataModels::FGetObjectsRequest Request;
    Request.Entity.Id = entityId;
    Request.Entity.Type = entityType;
    Request.EscapeObject = true;

    DataAPI->GetObjects(
        Request,
        PlayFab::UPlayFabDataAPI::FGetObjectsDelegate::CreateUObject(this, &UPlayFabCppTests::ObjectAPI_Success),
        PlayFab::FPlayFabErrorDelegate::CreateUObject(this, &UPlayFabCppTests::OnSharedError)
    );
}

void UPlayFabCppTests::ObjectAPI_Success(const PlayFab::DataModels::FGetObjectsResponse& result)
{
    CurrentTestContext->EndTest();
}
