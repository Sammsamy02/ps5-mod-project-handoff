import sys
sys.dont_write_bytecode = True
sys.path.insert(0, '/WORKSPACE/.codex/worktrees/translation-core/caps-team-A/caps-team-work/cap12-round6/work/tools')
sys.path.insert(0, '/WORKSPACE/.codex/worktrees/translation-core/caps-team-A/caps-team-work/cap12-round6/tests')
import invisible_route as ir
import test_round6 as t6
blob = b'\x24\x00\x00\x00FLocalVertexFactoryShaderParameters\x00\x26\x00\x00\x00FGPUSkinVertexFactoryShaderParameters\x00' \
       b'\x2f\x00\x00\x00TGPUSkinAPEXClothVertexFactoryShaderParameters\x00'
toks = sorted(ir.vf_tokens(blob))
print('vf_tokens on a master with the cloth VF:', toks)
om = {'master': '/G/M_Cloth', 'tokens': toks}
new, why = ir.decide_row(t6.row(), {'users': {'Blueprint': 3}, 'original_master': om}, t6.parents(), [t6.target()])
print('R3b result for Blueprint-only users of a cloth master:', new and new['action'], new and new['users_proven_by'], why)
print('glued token:', ir.vf_tokens(b'FooFLocalVertexFactoryShaderParameters'))
