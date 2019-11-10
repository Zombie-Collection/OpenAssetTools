﻿using System;
using System.Collections.Generic;
using System.Linq;
using ZoneCodeGenerator.Domain.FastFileStructure;
using ZoneCodeGenerator.Persistence;

namespace ZoneCodeGenerator.Domain.Information
{
    class StructureInformation
    {
        public DataTypeWithMembers Type { get; }
        public bool IsUnion => Type is DataTypeUnion;
        public FastFileBlock Block { get; set; }
        
        public EnumMember AssetEnumEntry { get; set; }
        public bool IsAsset => AssetEnumEntry != null;

        private int? fastFileAlign;
        public bool HasNonDefaultAlign => fastFileAlign != null;
        public int FastFileAlign
        {
            get => fastFileAlign ?? Type.Alignment;
            set => fastFileAlign = value;
        }
        
        public List<StructureInformation> Usages { get; }
        public List<MemberInformation> OrderedMembers { get; set; }
        public bool NonEmbeddedReferenceExists { get; set; }
        public bool SinglePointerReferenceExists { get; set; }
        public bool ArrayPointerReferenceExists { get; set; }
        public bool ArrayReferenceExists { get; set; }

        public bool HasNameMember => Type.Members.Any(variable => variable.Name.Equals("name", StringComparison.CurrentCultureIgnoreCase));
        
        public StructureInformation(DataTypeWithMembers type)
        {
            AssetEnumEntry = null;
            fastFileAlign = null;
            Type = type;
            NonEmbeddedReferenceExists = false;
            SinglePointerReferenceExists = false;
            ArrayReferenceExists = false;
            Usages = new List<StructureInformation>();
            OrderedMembers = new List<MemberInformation>();
        }
    }
}
