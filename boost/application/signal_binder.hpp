// common_application.hpp ----------------------------------------------------//
// -----------------------------------------------------------------------------

// Copyright 2011-2013 Renato Tegon Forti

// Distributed under the Boost Software License, Version 1.0.
// See http://www.boost.org/LICENSE_1_0.txt

// -----------------------------------------------------------------------------

// Revision History
// 26-10-2013 dd-mm-yyyy - Initial Release

// -----------------------------------------------------------------------------

#ifndef BOOST_APPLICATION_SIGNAL_MANAGER_HPP
#define BOOST_APPLICATION_SIGNAL_MANAGER_HPP

#include <boost/application/config.hpp>
#include <boost/application/context.hpp>
#include <boost/application/handler.hpp>

#include <boost/bind.hpp>
#include <boost/thread/thread.hpp>

#include <boost/application/aspects/termination_handler.hpp>
#include <boost/application/aspects/wait_for_termination_request.hpp>

namespace boost { namespace application {

   // todo : doxygen comment
   // This is an attempt to make things more flexible, 
   // this allow user to define your own sinal -> handler map
   class signal_binder
   {

   public:
      explicit signal_binder(context &cxt)
         : io_service_thread_(0)
         , signals_(io_service_)
         , context_(cxt)
      {
         signals_.async_wait(
            boost::bind(&signal_binder::signal_handler, this, 
            boost::asio::placeholders::error, 
            boost::asio::placeholders::signal_number));
      }

      virtual ~signal_binder()
      {
         if(io_service_thread_)
         {
            delete io_service_thread_;
         }
      }

      void bind(int signal_number, const handler& h)
      {
         boost::system::error_code ec;
         bind(signal_number, h, handler(), ec);

         if(ec) BOOST_APPLICATION_THROW_LAST_SYSTEM_ERROR("tie_signal() failed");
      }

      void bind(int signal_number, const handler& h1, const handler& h2)
      {
         boost::system::error_code ec;
         bind(signal_number, h1, h2, ec);

         if(ec) BOOST_APPLICATION_THROW_LAST_SYSTEM_ERROR("tie_signal() failed");
      }

      void bind(int signal_number, const handler& h, boost::system::error_code& ec)
      {
         bind(signal_number, h, handler(), ec);
      }

      void bind(int signal_number, const handler& h1, const handler& h2, boost::system::error_code& ec)
      {
         signals_.add(signal_number, ec);
         handler_map_[signal_number] = std::make_pair(h1, h2);
      }

      void unbind(int signal_number)
      {
         boost::system::error_code ec;
         unbind(signal_number, ec);

         if(ec) BOOST_APPLICATION_THROW_LAST_SYSTEM_ERROR("tie_signal() failed");
      }

      void unbind(int signal_number, boost::system::error_code& ec)
      {
         if(handler_map_.cend() != handler_map_.find(signal_number))
         {
            // replace
            signals_.remove(signal_number, ec);
            handler_map_.erase(signal_number);
         }
      }

      bool is_tied(int signal_number)
      {
         return (handler_map_.cend() != handler_map_.find(signal_number));
      }    
      
      void start()
      {
          io_service_thread_ = new boost::thread(
            boost::bind(&signal_binder::run_io_service, this));
      }

   protected:
  
      void run_io_service()
      {
         io_service_.run();
      }

      void signal_handler(const boost::system::error_code& ec, int signal_number)
      {
         if (!ec)
         {
            if(handler_map_[signal_number].first.parameter_callback_is_valid())
            {
               handler::parameter_callback* parameter;

               if(handler_map_[signal_number].first.callback(parameter))
               {
                  if((*parameter)(context_))
                  {
                     // user tell us to call second callback
                     if(handler_map_[signal_number].second.callback(parameter))
                        (*parameter)(context_);
                  }

                  return;
               }
            }

            if(handler_map_[signal_number].first.singleton_callback_is_valid())
            {
               handler::singleton_callback* singleton = 0;

               if(handler_map_[signal_number].first.callback(singleton))
               {
                  if((*singleton)())
                  {
                     // user tell us to call second callback
                     if(handler_map_[signal_number].second.callback(singleton))
                        (*singleton)();
                  }

                  return ;
               }
            }
         }
      }
   protected:
      
      application::context &context_;

   private:

      // signal < handler / handler> 
      // if first handler returns true, the second handler are called
      BOOST_APPLICATION_FEATURE_NS_SELECT::
         unordered_map<int, std::pair<handler, handler> > handler_map_;

      asio::io_service io_service_;
      asio::signal_set signals_;
      
      boost::thread *io_service_thread_; 
   };

   class signal_manager
      : public signal_binder
   {

   public:
   
      signal_manager(application::context &context, boost::system::error_code& ec) 
         : signal_binder(context)
      {
         register_signals(ec);
      }

      signal_manager(application::context &context) 
         : signal_binder(context)
      {
         boost::system::error_code ec;

         register_signals(ec);

         if(ec) BOOST_APPLICATION_THROW_LAST_SYSTEM_ERROR("signal_manager() failed");
      }

   protected:

      virtual void register_signals(boost::system::error_code& ec)
      {
         BOOST_APPLICATION_FEATURE_SELECT

         context_.add_aspect_if_not_exists<wait_for_termination_request>(
            shared_ptr<wait_for_termination_request>(
               new wait_for_termination_request_default_behaviour));

         if(context_.has_aspect<termination_handler>())
         {
            shared_ptr<termination_handler> th =
               context_.get_aspect<termination_handler>();

            handler::parameter_callback callback 
               = boost::bind<bool>(&signal_manager::termination_signal_handler, this, _1);

            bind(SIGINT,  th->get_handler(), callback, ec);
            if(ec) return;

            bind(SIGTERM, th->get_handler(), callback, ec);
            if(ec) return;

            bind(SIGABRT, th->get_handler(), callback, ec);
            if(ec) return;
         }
      }

      virtual bool termination_signal_handler(application::context &context)
      { 
         // we need set application_state to stop
         context_.use_aspect<status>().state(status::stoped);

         // and signalize wait_for_termination_request
         context_.use_aspect<wait_for_termination_request>().proceed();

         // this is not used
         return false;
      }
   };

}} // boost::application 

#endif // BOOST_APPLICATION_SIGNAL_MANAGER_HPP
       