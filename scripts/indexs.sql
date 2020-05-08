/*action_trace table index*/
CREATE INDEX IF NOT EXISTS index_action_trace_sequence ON action_trace ( sequence );
CREATE INDEX IF NOT EXISTS index_action_trace_contract_name ON action_trace USING hash ( act_account);
CREATE INDEX IF NOT EXISTS index_action_trace_action_name ON action_trace USING hash ( act_name);


/*transfer_t index*/
CREATE INDEX IF NOT EXISTS index_transfer_t_token_from ON transfer_t USING hash (token_from);
CREATE INDEX IF NOT EXISTS index_transfer_t_token_to ON transfer_t USING hash (token_to);
