/*action_trace table index*/
CREATE INDEX IF NOT EXISTS index_action_trace_sequence ON action_trace ( sequence );
CREATE INDEX IF NOT EXISTS index_action_trace_contract_name ON action_trace (creator_action_oridnal, act_account);
CREATE INDEX IF NOT EXISTS index_action_trace_action_name ON action_trace (creator_action_oridnal, act_name);


/*transaction_trace table index*/

