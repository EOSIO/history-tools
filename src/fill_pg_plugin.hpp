// copyright defined in LICENSE.txt

#pragma once
#include "fill_plugin.hpp"
#include "pg_plugin.hpp"
#include <appbase/application.hpp>

class fill_pg_plugin : public appbase::plugin<fill_pg_plugin> {
  public:
    APPBASE_PLUGIN_REQUIRES((pg_plugin)(fill_plugin))

    fill_pg_plugin();
    virtual ~fill_pg_plugin();

    virtual void set_program_options(appbase::options_description& cli, appbase::options_description& cfg) override;
    void         plugin_initialize(const appbase::variables_map& options);
    void         plugin_startup();
    void         plugin_shutdown();

  private:
    std::shared_ptr<struct fill_postgresql_plugin_impl> my;
};

template<std::size_t I = 0, typename FuncT, typename... Tp>
inline typename std::enable_if<I == sizeof...(Tp), void>::type
variant_for_each(std::variant<Tp...>, FuncT)
{ }

template<std::size_t I = 0, typename FuncT, typename... Tp>
inline typename std::enable_if<I < sizeof...(Tp), void>::type
variant_for_each(std::variant<Tp...> t, FuncT f)
{
    if (I == t.index())
        f(I, std::get<I>(t));
    else
        f(I, std::variant_alternative_t<I, decltype(t)>());
    variant_for_each<I + 1, FuncT, Tp...>(t, f);
}
