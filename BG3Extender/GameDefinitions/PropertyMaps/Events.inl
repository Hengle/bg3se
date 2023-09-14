BEGIN_CLS(lua::EventBase)
P_RO(Name)
P_RO(CanPreventAction)
P_RO(ActionPrevented)
P_RO(Stopped)
P_FUN(StopPropagation, StopPropagation)
P_FUN(PreventAction, PreventAction)
END_CLS()

BEGIN_CLS(lua::EmptyEvent)
INHERIT(lua::EventBase)
END_CLS()

BEGIN_CLS(ecl::lua::GameStateChangeEvent)
INHERIT(lua::EventBase)
P_RO(FromState)
P_RO(ToState)
END_CLS()

BEGIN_CLS(esv::lua::GameStateChangeEvent)
INHERIT(lua::EventBase)
P_RO(FromState)
P_RO(ToState)
END_CLS()

BEGIN_CLS(lua::TickEvent)
INHERIT(lua::EventBase)
P(Time)
END_CLS()

BEGIN_CLS(esv::lua::DoConsoleCommandEvent)
INHERIT(lua::EventBase)
P_RO(Command)
END_CLS()

BEGIN_CLS(esv::lua::DealDamageEvent)
INHERIT(lua::EventBase)
P(Functor)
P(Caster)
P(Target)
P(Position)
P(IsFromItem)
P(SpellId)
P(StoryActionId)
P(Originator)
P(Hit)
P(DamageSums)
P(HitWith)
END_CLS()

BEGIN_CLS(esv::lua::ExecuteFunctorEvent)
INHERIT(lua::EventBase)
P(Functor)
P(Params)
END_CLS()

BEGIN_CLS(esv::lua::AfterExecuteFunctorEvent)
INHERIT(lua::EventBase)
P(Functor)
P(Params)
P(Hit)
END_CLS()
