/*
 * Copyright (c) 2019 Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ResourceBroker.h"
#include "CloudConnectTrace.h"
#include "DBusAdapter.h"
#include "MblCloudClient.h"
#include "mbed-cloud-client/MbedCloudClient.h"

#include <cassert>
#include <pthread.h>

#define TRACE_GROUP "ccrb"

namespace mbl
{

// Currently, this constructor is called from MblCloudClient thread.
ResourceBroker::ResourceBroker() : cloud_client_(nullptr), registration_in_progress_(false)
{
    TR_DEBUG("Enter");
}

ResourceBroker::~ResourceBroker()
{
    TR_DEBUG("Enter");
}

MblError ResourceBroker::start(MbedCloudClient* cloud_client)
{
    TR_DEBUG("Enter");
    assert(cloud_client);
    cloud_client_ = cloud_client;

    // create new thread which will run IPC event loop
    const int thread_create_err =
        pthread_create(&ipc_thread_id_,
                       nullptr, // thread is created with default attributes
                       ResourceBroker::ccrb_main,
                       this);
    if (0 != thread_create_err) {
        // thread creation failed, print errno value and exit
        TR_ERR("Thread creation failed (%s)", strerror(errno));
        return Error::CCRBStartFailed;
    }

    // thread was created
    return Error::None;
}

MblError ResourceBroker::stop()
{
    TR_DEBUG("Enter");

    // FIXME: handle properly all errors in this function.

    assert(ipc_adapter_);

    // try sending stop signal to ipc (pass no error)
    const MblError ipc_stop_err = ipc_adapter_->stop(MblError::None);
    if (Error::None != ipc_stop_err) {
        TR_ERR("ipc::stop failed! (%s)", MblError_to_str(ipc_stop_err));

        // FIXME: Currently, if ipc was not successfully signalled, we return error.
        //        Required to add "release resources best effort" functionality.
        return Error::CCRBStopFailed;
    }

    // about thread_status: thread_status is the variable that will contain ccrb_main status.
    // ccrb_main returns MblError type. pthread_join will assign the value returned via
    // pthread_exit to the value of pointer thread_status. For example, if ccrb_main
    // will return N via pthread_exit(N), value of pointer thread_status will be equal N.
    void* thread_status = nullptr; // do not dereference the pointer!

    // ipc was succesfully signalled to stop, join with thread.
    const int thread_join_err = pthread_join(ipc_thread_id_, &thread_status);
    // FIXME: Currently, we use pthread_join without timeout.
    //        Required to use pthread_join with timeout.
    if (0 != thread_join_err) {
        // thread joining failed, print errno value
        TR_ERR("Thread joining failed (%s)", strerror(errno));

        // FIXME: Currently, if pthread_join fails, we return error.
        //        Required to add "release resources best effort" functionality.
        return Error::CCRBStopFailed;
    }

    // thread was succesfully joined, handle thread_status. Thread_status will contain
    // value that was returned via pthread_Exit(MblError) from ccrb main function.
    MblError* ret_value = static_cast<MblError*>(thread_status);
    if (Error::None != (*ret_value)) {
        // the return with an error ret_Value indicate of the reason for exit, but not the
        // failure of the current function.
        TR_ERR("ccrb_main() exit with error (*ret_value)= %s", MblError_to_str((*ret_value)));
    }

    const MblError de_init_err = deinit();
    if (Error::None != de_init_err) {
        TR_ERR("ccrb::de_init failed! (%s)", MblError_to_str(de_init_err));
    }

    return de_init_err;
}

MblError ResourceBroker::init()
{
    TR_DEBUG("Enter");

    // verify that ipc_adapter_ member was not created yet
    assert(nullptr == ipc_adapter_);

    // create ipc instance and pass ccrb instance to constructor
    ipc_adapter_ = std::make_unique<DBusAdapter>(*this);

    MblError status = ipc_adapter_->init();
    if (Error::None != status) {
        TR_ERR("ipc::init failed with error %s", MblError_to_str(status));
    }

    // Set function pointers to point to mbed_client functions
    // In gtest our friend test class will override these pointers to get all the
    // calls
    register_update_func_ = std::bind(&MbedCloudClient::register_update, cloud_client_);
    add_objects_func_ = std::bind(
        static_cast<void (MbedCloudClient::*)(const M2MObjectList&)>(&MbedCloudClient::add_objects),
        cloud_client_,
        std::placeholders::_1);

    // Register Cloud client callback:
    regsiter_callback_handlers();

    return status;
}

MblError ResourceBroker::deinit()
{
    TR_DEBUG("Enter");

    assert(ipc_adapter_);

    // FIXME: Currently we call ipc_adapter_->de_init unconditionally.
    //        ipc_adapter_->de_init can't be called if ccrb thread was not finished.
    //        Required to call ipc_adapter_->de_init only if ccrb thread was finished.
    MblError status = ipc_adapter_->deinit();
    if (Error::None != status) {
        TR_ERR("ipc::de_init failed with error %s", MblError_to_str(status));
    }

    return status;
}

MblError ResourceBroker::run()
{
    TR_DEBUG("Enter");
    MblError stop_status;

    assert(ipc_adapter_);

    MblError status = ipc_adapter_->run(stop_status);
    if (Error::None != status) {
        TR_ERR("ipc::run failed with error %s", MblError_to_str(status));
    }
    else
    {
        TR_ERR("ipc::run stopped with status %s", MblError_to_str(stop_status));
    }

    return status;
}

void* ResourceBroker::ccrb_main(void* ccrb)
{
    TR_DEBUG("Enter");

    assert(ccrb);

    ResourceBroker* const this_ccrb = static_cast<ResourceBroker*>(ccrb);

    MblError status = this_ccrb->init();
    if (Error::None != status) {
        TR_ERR("ccrb::init failed with error %s. Exit CCRB thread.", MblError_to_str(status));
        pthread_exit((void*) (uintptr_t) status);
    }

    status = this_ccrb->run();
    if (Error::None != status) {
        TR_ERR("ccrb::run failed with error %s. Exit CCRB thread.", MblError_to_str(status));
        // continue to deinit and return status
    }

    MblError status1 = this_ccrb->deinit();
    if (Error::None != status1) {
        TR_ERR("ccrb::deinit failed with error %s. Exit CCRB thread.", MblError_to_str(status1));
        pthread_exit((void*) &status1);
    }

    TR_INFO("thread function finished");
    pthread_exit((void*) (uintptr_t) status); // pthread_exit does "return"
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Callback functions that are being called by Mbed client
////////////////////////////////////////////////////////////////////////////////////////////////////

void ResourceBroker::regsiter_callback_handlers()
{
    TR_DEBUG("Enter");

    // Resource broker will now get Mbed cloud client callbacks on the following
    // cases:
    // 1. Registration update
    // 2. Error occurred

    if (nullptr != cloud_client_) {
        // GTest unit test might set cloud_client_ member to nullptr
        cloud_client_->on_registration_updated(this,
                                               &ResourceBroker::handle_registration_updated_cb);
        cloud_client_->on_error(this, &ResourceBroker::handle_error_cb);
    }
}

void ResourceBroker::handle_registration_updated_cb()
{
    TR_DEBUG("Enter");
    // Mark that registration is finished (using atomic flag)
    registration_in_progress_.store(false);
}

void ResourceBroker::handle_error_cb(const int cloud_client_code)
{
    TR_DEBUG("Enter");
    const MblError mbl_code =
        CloudClientError_to_MblError(static_cast<MbedCloudClient::Error>(cloud_client_code));
    TR_ERR("Error occurred: %d: %s", mbl_code, MblError_to_str(mbl_code));

    // Mark that registration is finished (using atomic flag)
    registration_in_progress_.store(false);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Callback functions that are being called by ApplicationEndpoint Class
////////////////////////////////////////////////////////////////////////////////////////////////////

void ResourceBroker::handle_app_register_update_finished_cb(const uintptr_t ipc_conn_handle,
                                                            const std::string& access_token)
{
    TR_DEBUG("Application (access_token: %s) registered successfully.", access_token.c_str());

    // Send the response to adapter:
    CloudConnectStatus reg_status = CloudConnectStatus::STATUS_SUCCESS;
    MblError status = ipc_adapter_->update_registration_status(ipc_conn_handle, reg_status);
    if (Error::None != status) {
        TR_ERR("update_registration_status failed with error: %s", MblError_to_str(status));
    }

    // Return registration updated callback to CCRB
    regsiter_callback_handlers();

    // Mark that registration is finished (using atomic flag)
    registration_in_progress_.store(false);
}

void ResourceBroker::handle_app_error_cb(const uintptr_t ipc_conn_handle,
                                         const std::string& access_token,
                                         const MblError error)
{
    TR_DEBUG("Application (access_token: %s) encountered an error: %s",
             access_token.c_str(),
             MblError_to_str(error));

    auto itr = app_endpoints_map_.find(access_token);
    if (app_endpoints_map_.end() == itr) {
        // Could not found application endpoint
        TR_ERR("Application (access_token: %s) does not exist.", access_token.c_str());
        // Mark that registration is finished (using atomic flag), even if it was failed
        registration_in_progress_.store(false);
        return;
    }

    ApplicationEndpoint_ptr app_endpoint = itr->second;

    // Send the response to adapter:
    if (app_endpoint->is_registered()) {
        // TODO: add call to update_deregistration_status when deregister is
        // implemented
        TR_ERR("Application (access_token: %s) is already registered.", access_token.c_str());
    }
    else
    {
        // App is not registered yet, which means the error is for register request
        MblError status = ipc_adapter_->update_registration_status(ipc_conn_handle,
                                                                   CloudConnectStatus::ERR_FAILED);
        if (Error::None != status) {
            TR_ERR("Registration for Application (access_token: %s), failed with error: %s",
                   access_token.c_str(),
                   MblError_to_str(status));
        }

        TR_DEBUG("Erase Application (access_token: %s)", access_token.c_str());
        app_endpoints_map_.erase(itr); // Erase endpoint as registation failed

        // Mark that registration is finished (using atomic flag) (even if it was failed)
        registration_in_progress_.store(false);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

MblError ResourceBroker::register_resources(const uintptr_t ipc_conn_handle,
                                            const std::string& app_resource_definition_json,
                                            CloudConnectStatus& out_status,
                                            std::string& out_access_token)
{
    TR_DEBUG("Enter");

    if (registration_in_progress_.load()) {
        // We only allow one registration request at a time.
        TR_ERR("Registration is already in progess.");
        out_status = CloudConnectStatus::ERR_REGISTRATION_ALREADY_IN_PROGRESS;
        return Error::None;
    }

    // Above check makes sure there is no registration in progress.
    // Lets check if an app is already not registered...
    // TODO: remove this check when supporting multiple applications
    if (!app_endpoints_map_.empty()) {
        // Currently support only ONE application
        TR_ERR("Only one registered application is allowed.");
        out_status = CloudConnectStatus::ERR_ALREADY_REGISTERED;
        return Error::None;
    }

    // Create and init Application Endpoint:
    // parse app_resource_definition_json and create unique access token
    ApplicationEndpoint_ptr app_endpoint = std::make_shared<ApplicationEndpoint>(ipc_conn_handle);
    const MblError status = app_endpoint->init(app_resource_definition_json);
    if (Error::None != status) {
        TR_ERR("app_endpoint->init failed with error: %s", MblError_to_str(status));
        out_status = (Error::CCRBInvalidJson == status)
                         ? CloudConnectStatus::ERR_INVALID_APPLICATION_RESOURCES_DEFINITION
                         : CloudConnectStatus::ERR_FAILED;
        return Error::None;
    }

    // Register application endpoint function
    app_endpoint->register_callback_functions(
        std::bind(&ResourceBroker::handle_app_register_update_finished_cb,
                  this,
                  std::placeholders::_1,
                  std::placeholders::_2),
        std::bind(&ResourceBroker::handle_app_error_cb,
                  this,
                  std::placeholders::_1,
                  std::placeholders::_2,
                  std::placeholders::_3));

    out_access_token = app_endpoint->get_access_token();

    // Set atomic flag for registration in progress
    registration_in_progress_.store(true);

    // Register the next cloud client callbacks to app_endpoint
    if (nullptr != cloud_client_) {
        // GTest unit test might set cloud_client_ member to nullptr
        cloud_client_->on_registration_updated(
            &(*app_endpoint), &ApplicationEndpoint::handle_registration_updated_cb);
        cloud_client_->on_error(&(*app_endpoint), &ApplicationEndpoint::handle_error_cb);
    }

    app_endpoints_map_[out_access_token] = app_endpoint; // Add application endpoint to map

    // Call Mbed cloud client to start registration update
    add_objects_func_(app_endpoint->get_m2m_object_list());
    register_update_func_();

    out_status = CloudConnectStatus::STATUS_SUCCESS;

    return Error::None;
}

MblError ResourceBroker::deregister_resources(const uintptr_t /*ipc_conn_handle*/,
                                              const std::string& /*access_token*/,
                                              CloudConnectStatus& /*out_status*/)
{

    TR_DEBUG("Enter");
    // empty for now
    return Error::CCRBNotSupported;
}

MblError ResourceBroker::add_resource_instances(const uintptr_t /*unused*/,
                                                const std::string& /*unused*/,
                                                const std::string& /*unused*/,
                                                const std::vector<uint16_t>& /*unused*/,
                                                CloudConnectStatus& /*unused*/)
{
    TR_DEBUG("Enter");
    // empty for now
    return Error::None;
}

MblError ResourceBroker::remove_resource_instances(const uintptr_t /*unused*/,
                                                   const std::string& /*unused*/,
                                                   const std::string& /*unused*/,
                                                   const std::vector<uint16_t>& /*unused*/,
                                                   CloudConnectStatus& /*unused*/)
{
    TR_DEBUG("Enter");
    // empty for now
    return Error::None;
}

MblError ResourceBroker::set_resources_values(const std::string& /*unused*/,
                                              std::vector<ResourceSetOperation>& /*unused*/,
                                              CloudConnectStatus& /*unused*/)
{
    TR_DEBUG("Enter");
    // empty for now
    return Error::None;
}

MblError ResourceBroker::get_resources_values(const std::string& /*unused*/,
                                              std::vector<ResourceGetOperation>& /*unused*/,
                                              CloudConnectStatus& /*unused*/)
{
    TR_DEBUG("Enter");
    // empty for now
    return Error::None;
}

} // namespace mbl