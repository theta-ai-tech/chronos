#include "chronos/accounting/valuation.hpp"
#include <algorithm>
#include <limits>
namespace chronos::accounting {
namespace {
using contracts::AmountUnits;
bool add(AmountUnits &a, AmountUnits b) { AmountUnits next{}; if (__builtin_add_overflow(a,b,&next)) return false; a=next; return true; }
bool magnitude(AmountUnits a, AmountUnits &b) { if(a==std::numeric_limits<AmountUnits>::min())return false; b=a<0?-a:a; return true; }
bool balanced(const LedgerTransaction &tx) {
 __int128 qty=0,money=0,position=0,fee=0;
 for(const auto &e:tx.entries) {
  if(e.unit==LedgerUnit::BaseQuantity)qty+=e.amount_units; else if(e.unit==LedgerUnit::QuoteCurrency)money+=e.amount_units; else return false;
  if(e.account==LedgerAccount::PositionQuantity) { if(e.unit!=LedgerUnit::BaseQuantity)return false; position+=e.amount_units; }
  if(e.account==LedgerAccount::FeeExpense) { if(e.unit!=LedgerUnit::QuoteCurrency)return false; fee+=e.amount_units; }
 }
 return qty==0 && money==0 && position==tx.signed_fill_quantity_units && fee==tx.fee_units;
}
}
PositionResult derive_position(std::span<const LedgerTransaction> history,const LedgerPolicy &scope) {
 std::vector<contracts::LedgerTransactionId> reversed;
 for(std::size_t i=0;i<history.size();++i) {
  const auto &t=history[i];
  if(t.policy!=scope)return {ValuationFailure::ScopeMismatch,{}};
  if(!balanced(t))return {ValuationFailure::InvalidHistory,{}};
  for(std::size_t j=0;j<i;++j)if(history[j].transaction_id==t.transaction_id)return {ValuationFailure::InvalidHistory,{}};
  if(t.kind==LedgerTransactionKind::Compensation) {
   if(!t.original_transaction_id || !t.correction_id || std::find(reversed.begin(),reversed.end(),*t.original_transaction_id)!=reversed.end())return {ValuationFailure::InvalidHistory,{}};
   const auto original=std::find_if(history.begin(),history.begin()+static_cast<std::ptrdiff_t>(i),[&](const auto &x){return x.transaction_id==*t.original_transaction_id;});
   if(original==history.begin()+static_cast<std::ptrdiff_t>(i) || original->kind!=LedgerTransactionKind::Fill || original->entries.size()!=t.entries.size() || static_cast<__int128>(original->signed_fill_quantity_units)+t.signed_fill_quantity_units!=0 || static_cast<__int128>(original->trade_notional_units)+t.trade_notional_units!=0 || static_cast<__int128>(original->fee_units)+t.fee_units!=0)return {ValuationFailure::InvalidHistory,{}};
   for(std::size_t j=0;j<t.entries.size();++j) if(t.entries[j].account!=original->entries[j].account || t.entries[j].unit!=original->entries[j].unit || static_cast<__int128>(t.entries[j].amount_units)+original->entries[j].amount_units!=0)return {ValuationFailure::InvalidHistory,{}};
   reversed.push_back(*t.original_transaction_id);
  } else if(t.kind!=LedgerTransactionKind::Fill || t.original_transaction_id || t.correction_id || t.trade_notional_units<0 || t.fee_units<0 || t.signed_fill_quantity_units==0)return {ValuationFailure::InvalidHistory,{}};
 }
 PositionProjection p;
 for(const auto &t:history) {
  if(t.kind!=LedgerTransactionKind::Fill || std::find(reversed.begin(),reversed.end(),t.transaction_id)!=reversed.end())continue;
  AmountUnits remaining{};
  if(!magnitude(t.signed_fill_quantity_units,remaining) || !add(p.position_units,t.signed_fill_quantity_units) || !add(p.fees_units,t.fee_units))return {ValuationFailure::ArithmeticOverflow,{}};
  AmountUnits money=t.trade_notional_units, event_profit=0; bool closed=false;
  const bool buy=t.signed_fill_quantity_units>0;
  while(remaining>0 && !p.lots.empty() && (p.lots.front().signed_quantity_units>0)!=buy) {
   auto &lot=p.lots.front(); AmountUnits lot_qty{};
   if(!magnitude(lot.signed_quantity_units,lot_qty))return {ValuationFailure::ArithmeticOverflow,{}};
   const auto close=std::min(remaining,lot_qty);
   // Floor each partial allocation; the final close receives all remainder.
   // A partial close receives floor allocation. The final close consumes the
   // exact remainder so repeated rounding cannot lose a quote unit.
   const auto basis=close == lot_qty ? lot.basis_units : static_cast<AmountUnits>(static_cast<__int128>(lot.basis_units)*close/lot_qty);
   const auto proceeds=close == remaining ? money : static_cast<AmountUnits>(static_cast<__int128>(money)*close/remaining);
   const auto profit=buy?basis-proceeds:proceeds-basis;
   if(!add(p.realized_gross_units,profit) || !add(event_profit,profit))return {ValuationFailure::ArithmeticOverflow,{}};
   closed=true; remaining-=close; money-=proceeds; lot.basis_units-=basis;
   lot.signed_quantity_units+=buy?close:-close;
   if(lot.signed_quantity_units==0)p.lots.erase(p.lots.begin());
  }
  if(closed) { ++p.closing_events; if(event_profit>0)++p.profitable_gross_closing_events; }
  if(remaining>0)p.lots.push_back({buy?remaining:-remaining,money});
 }
 for(const auto &lot:p.lots)if(!add(p.remaining_basis_units,lot.basis_units))return {ValuationFailure::ArithmeticOverflow,{}};
 return {ValuationFailure::None,std::move(p)};
}
ValuationResult value_position(std::span<const LedgerTransaction> history,const std::optional<contracts::PositionMark> &mark,const ValuationPolicy &policy,ValuationCut cut) {
 auto position=derive_position(history,policy.scope);
 ValuationResult result{position.failure,std::move(position.position),{}, {}};
 if(!result.position)return result;
 if(!mark) {result.failure=ValuationFailure::MissingMark;return result;}
 const auto &m=*mark; const auto &s=policy.scope;
 const __int128 age=static_cast<__int128>(cut.logical_time_nanoseconds)-m.logical_time_nanoseconds;
 if(m.run_id!=s.run_id || m.portfolio_id!=s.portfolio_id || m.account_id!=s.account_id || m.canonical_instrument_id!=s.canonical_instrument_id || m.listing_id!=s.listing_id || m.quote_currency!=s.quote_currency || m.run_mode!=s.run_mode || m.quantity_scale!=s.quantity_scale || m.money_scale!=s.money_scale || m.mark_policy_version!=policy.mark_policy_version || m.price_units<=0 || m.quality.status()!=contracts::QualityStatus::valid || m.run_input_sequence>cut.run_input_sequence || age<0 || policy.maximum_mark_age_nanoseconds<0 || age>policy.maximum_mark_age_nanoseconds) {result.failure=ValuationFailure::InvalidMark;return result;}
 AmountUnits unrealized=0;
 for(const auto &lot:result.position->lots) {
  AmountUnits qty{}; if(!magnitude(lot.signed_quantity_units,qty)) {result.failure=ValuationFailure::ArithmeticOverflow;return result;}
  // Reduce scales before multiplication to stay inside signed 128-bit range.
  const int exponent=static_cast<int>(m.money_scale.exponent())-static_cast<int>(m.price_scale.exponent())-static_cast<int>(m.quantity_scale.exponent());
  __int128 numerator=static_cast<__int128>(qty)*m.price_units, denominator=1;
  bool overflow=false;
  if(exponent>=0) for(int i=0;i<exponent;++i) { __int128 next{}; if(__builtin_mul_overflow(numerator,static_cast<__int128>(10),&next)){overflow=true;break;} numerator=next; }
  else for(int i=0;i<-exponent;++i)denominator*=10;
  if(overflow) {result.failure=ValuationFailure::ArithmeticOverflow;return result;}
  __int128 value=numerator/denominator+(numerator%denominator!=0?1:0);
  if(value>std::numeric_limits<AmountUnits>::max()) {result.failure=ValuationFailure::ArithmeticOverflow;return result;}
  const auto notional=static_cast<AmountUnits>(value);
  if(!add(unrealized,lot.signed_quantity_units>0?notional-lot.basis_units:lot.basis_units-notional)) {result.failure=ValuationFailure::ArithmeticOverflow;return result;}
 }
 AmountUnits net=result.position->realized_gross_units;
 if(!add(net,unrealized) || !add(net,-result.position->fees_units)) {result.failure=ValuationFailure::ArithmeticOverflow;return result;}
 result.unrealized_gross_units=unrealized; result.total_net_units=net; return result;
}
}
